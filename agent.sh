#!/usr/bin/env bash
# andy-agent — Startskript mit Server-Abfrage und Persona-Auswahl

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTS_DIR="$SCRIPT_DIR/personas"
SKILLS_DIR="$SCRIPT_DIR/skills"
GLOBAL_AGENTS_MD="$HOME/.andy-agent/AGENTS.md"
PERSONA=""

# --- Persona-Auswahl ---
select_persona() {
  local -a personas=()
  local -a names=()

  # .md-Dateien aus personas/ auflisten
  for f in "$AGENTS_DIR"/*.md; do
    [[ -f "$f" ]] || continue
    local name
    name="$(basename "$f" .md)"
    personas+=("$name")
    names+=("$(echo "$name" | sed 's/-/ /g' | sed 's/\b\(.\)/\u\1/g')")
  done

  echo ""
  echo "Persona auswählen:"
  echo ""
  for i in "${!names[@]}"; do
    printf "  %2d) %s\n" "$((i + 1))" "${names[$i]}"
  done
  echo ""

  while true; do
    read -rp "  Auswahl [1-${#names[@]}]: " choice
    if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#names[@]} )); then
      PERSONA="${personas[$((choice - 1))]}"
      return
    fi
    echo "  Ungültige Nummer."
  done
}

# --- Persona-Skill-Bindung ---
# Wenn eine Persona ausgewählt wird, wird die SKILL.md nach
# ~/.andy-agent/skills/SKILL.md kopiert.
# andy-agent parst nur ~/.andy-agent/skills/ und findet so
# nur den Skill der ausgewählten Persona.
bind_persona_skills() {
  local persona_name="$1"
  local skill_file="$SKILLS_DIR/$persona_name/SKILL.md"
  local global_skills_tmp="$HOME/.andy-agent/skills"

  # Alte SKILL.md entfernen (falls vorhanden)
  rm -f "$global_skills_tmp/SKILL.md"

  if [[ -f "$skill_file" ]]; then
    mkdir -p "$global_skills_tmp"
    cp "$skill_file" "$global_skills_tmp/SKILL.md"
    echo "  → Skill kopiert: $persona_name → $global_skills_tmp/SKILL.md"
  else
    echo "  → Keine SKILL.md für $persona_name gefunden"
  fi
}

# --- Server-Abfrage ---
DEFAULT_HOST="${LLAMA_SERVER_HOST:-127.0.0.1}"
DEFAULT_PORT="${LLAMA_SERVER_PORT:-8081}"

read -rp "Hostname/IP [${DEFAULT_HOST}]: " HOST_INPUT
HOST="${HOST_INPUT:-$DEFAULT_HOST}"

read -rp "Port [${DEFAULT_PORT}]: " PORT_INPUT
PORT="${PORT_INPUT:-$DEFAULT_PORT}"

if [[ ! "$PORT" =~ ^[0-9]+$ ]] || (( PORT < 1 || PORT > 65535 )); then
  echo "Ungültiger Port. Muss eine Nummer zwischen 1 und 65535 sein."
  exit 1
fi

SERVER_URL="http://${HOST}:${PORT}"

# --- Arbeitsverzeichnis-Abfrage ---
DEFAULT_CWD="${ANDY_CWD:-$(pwd)}"
read -rp "Arbeitsverzeichnis [${DEFAULT_CWD}]: " CWD_INPUT
CWD="${CWD_INPUT:-$DEFAULT_CWD}"

if [[ ! -d "$CWD" ]]; then
  echo "Verzeichnis existiert nicht: $CWD"
  exit 1
fi
CWD="$(cd "$CWD" && pwd)"  # canonical path

# --- Persona auswählen (direkt, keine Subshell) ---
select_persona
PERSONA_NAME=$(echo "$PERSONA" | sed 's/-/ /g' | sed 's/\b\(.\)/\u\1/g')

# --- Persona-Skill-Bindung aktivieren ---
bind_persona_skills "$PERSONA"

echo ""
echo "Persona: $PERSONA_NAME"
echo "Server: $SERVER_URL"
echo ""

# --- Persona als globale AGENTS.md aktivieren ---
if [[ -f "$AGENTS_DIR/$PERSONA.md" ]]; then
  mkdir -p "$HOME/.andy-agent"
  rm -f "$GLOBAL_AGENTS_MD"
  cp "$AGENTS_DIR/$PERSONA.md" "$GLOBAL_AGENTS_MD"
fi

# --- Start ---
exec ./build/andy-agent \
  --url "$SERVER_URL" \
  --cwd "$CWD" \
  "$@"
