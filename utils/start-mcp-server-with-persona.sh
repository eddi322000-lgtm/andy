#!/usr/bin/env bash
# Wrapper für start-mcp-server.sh mit Persona-Auswahl

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PERSONAS_DIR="${PROJECT_ROOT}/personas"
SKILLS_DIR="${PROJECT_ROOT}/skills"

select_persona() {
  local -a personas=()
  local -a names=()
  for f in "${PERSONAS_DIR}"/*.md; do
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
      PERSONA_NAME="${names[$((choice - 1))]}"
      break
    fi
    echo "  Ungültige Nummer."
  done
}

bind_persona_skills() {
  local persona_name="$1"
  local skill_file="${SKILLS_DIR}/${persona_name}/SKILL.md"
  local skill_dest="$HOME/.andy-agent/skills"
  rm -f "${skill_dest}/SKILL.md"
  if [[ -f "$skill_file" ]]; then
    mkdir -p "$skill_dest"
    cp "$skill_file" "$skill_dest/SKILL.md"
    echo "Skill "$persona_name" aktiviert"
  else
    echo "Keine SKILL.md für $persona_name gefunden"
  fi
}

HOST=${1:-0.0.0.0}
PORT=${2:-31234}

echo "Persona-Auswahl für andy-mcp-server"
select_persona
bind_persona_skills "$PERSONA"

echo "Starte MCP-Server mit Persona '${PERSONA_NAME}' auf ${HOST}:${PORT}"
"${SCRIPT_DIR}/start-mcp-server.sh" "$HOST" "$PORT"