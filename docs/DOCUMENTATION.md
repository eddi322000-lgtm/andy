# andy-agent-next

## Inhaltsverzeichnis

1. [Einführung](#1-einführung)
2. [Build-Anleitung](#2-build-anleitung)
3. [Nutzung](#3-nutzung)
4. [Personas und Skills](#4-personas-und-skills)
5. [Sicherheit](#5-sicherheit)
6. [Session-Management](#6-session-management)
7. [MCP Server](#7-mcp-server)
8. [Architektur](#8-architektur)
9. [Entwickler-Guide](#9-entwickler-guide)
10. [Fehlerbehebung](#10-fehlerbehebung)

---

## 1. Einführung

`andy-agent` ist ein lokaler, hochgradig anpassbarer KI-Assistent für die Code-Entwicklung. Er läuft vollständig offline, verursacht keine API-Kosten und sendet keine Telemetrie-Daten.

### Kernkonzept

Der Agent arbeitet in einer kontinuierlichen Schleife (**Agent Loop**):
1. Benutzer-Eingabe erhalten
2. Mit einem LLM (z.B. `llama.cpp`) interagieren
3. Bei Bedarf Werkzeuge (**Tools**) ausführen
4. Wiederholen, bis eine finale Antwort generiert wurde

### Voraussetzungen

- **Compiler**: g++ 9+ oder clang++ 9+ (C++17-fähig)
- **Systembibliotheken**: pthread (Linux/Unix)
- **Abhängigkeiten**: Alle im `vendor/`-Verzeichnis enthalten (keine externe Installation nötig)

| Bibliothek | Pfad | Beschreibung |
|------------|------|-------------|
| cpp-httplib | `vendor/cpp-httplib/` | HTTP-Client für llama-server Kommunikation |
| nlohmann/json | `vendor/nlohmann/` | JSON-Parser |
| stb_image | `vendor/stb/` | Bild-Unterstützung (Clipboard-Paste) |

---

## 2. Build-Anleitung

### CMake

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Die Binary liegt in `build/andy-agent`.

**Reinigen (CMake):**
```bash
rm -rf build
```

### Manuelles Kompilieren

Falls Make oder CMake nicht verfügbar sind:

```bash
CXX=g++
CXXFLAGS="-std=c++17 -Wall -Wextra -O2"
INCLUDES="-I./common -I./tools -I./vendor -I./vendor/cpp-httplib -I./vendor/stb -I./include"

for f in \
    common/common.cpp common/chat.cpp common/chat-data.cpp \
    common/log.cpp common/console.cpp common/peg-parser.cpp \
    common/unicode.cpp common/json-schema-to-grammar.cpp \
    common/jinja/caps.cpp common/jinja/lexer.cpp common/jinja/parser.cpp \
    common/jinja/runtime.cpp common/jinja/string.cpp common/jinja/value.cpp \
    tools/agent.cpp tools/agent-loop.cpp tools/agent-loop-compaction.cpp \
    tools/agent-loop-completion-cli.cpp tools/agent-loop-completion.cpp \
    tools/agent-loop-run.cpp tools/agent-loop-tools-cli.cpp tools/agent-loop-tools.cpp \
    tools/http-inference-backend.cpp tools/agent-resources.cpp \
    tools/clipboard-image.cpp tools/stb-image-impl.cpp \
    tools/tool-registry.cpp tools/permission.cpp tools/permission-async.cpp \
    tools/permission-policy.cpp tools/permission-prompt.cpp \
    tools/compaction.cpp tools/session-file.cpp tools/agent-tool-parser.cpp \
    tools/mcp/mcp-server-manager.cpp tools/mcp/mcp-tool-wrapper.cpp \
    tools/mcp/mcp-client.cpp \
    tools/agents-md/agents-md-manager.cpp tools/skills/skills-manager.cpp \
    tools/tools/tool-bash.cpp tools/tools/tool-edit.cpp tools/tools/tool-glob.cpp \
    tools/tools/tool-plan.cpp tools/tools/tool-read.cpp tools/tools/tool-write.cpp \
    vendor/cpp-httplib/httplib.cpp; do
    $CXX $CXXFLAGS $INCLUDES -c -o "${f%.cpp}.o" "$f"
done

$CXX $CXXFLAGS -o andy-agent *.o -lpthread
```

### SSL-Unterstützung (für MCP mit HTTPS)

```bash
cmake -B build -DLLAMA_BUILD_LIBRESSL=ON
cmake --build build -t andy-agent -j
```

### Testen

```bash
./andy-agent --help
```

---

## 3. Nutzung

### Starten

```bash
# Mit Startskript (Server-Abfrage + Persona-Auswahl)
./agent.sh

# Direkt
./build/andy-agent --url http://localhost:8081
```

#### Startskript (`agent.sh`)

Das Startskript führt folgende Schritte aus:

1. **Server-Abfrage**: Fragt Host und Port ab (Standard: `127.0.0.1:8081`). Überschreibbar über Environment-Variablen:
   - `LLAMA_SERVER_HOST` — Server-Hostname (Default: `127.0.0.1`)
   - `LLAMA_SERVER_PORT` — Server-Port (Default: `8081`)
2. **Persona-Auswahl**: Listet alle `.md`-Dateien in `personas/` auf
3. **Persona-Skill-Bindung**: Kopiert `skills/<persona>/SKILL.md` nach `~/.andy-agent/skills/SKILL.md`
4. **Persona als AGENTS.md**: Kopiert `personas/<persona>.md` nach `~/.andy-agent/AGENTS.md`
5. **Start**: Führt `./build/andy-agent --url <SERVER_URL>` aus

**Wichtig:** Im Startskript sind Persona und AGENTS.md verknüpft — die ausgewählte Persona **ersetzt** die globale AGENTS.md.

### Interaktive Nutzung

Im CLI-Modus kannst du normal chatten. Der Agent führt auch selbstständig Befehle aus (Dateien lesen, Code schreiben, etc.).

#### Slash-Commands

| Befehl | Beschreibung |
|---|---|
| `/exit` | Beendet die Sitzung und das Programm |
| `/clear` | Löscht den Chat-Verlauf |
| `/tools` | Zeigt verfügbare Tools |
| `/skills` | Zeigt verfügbare Skills |
| `/agents` | Listet entdeckte AGENTS.md-Dateien |
| `/stats` | Token-Nutzung und Effizienz |
| `/compact` | Erzwingt Kontext-Komprimierung |

### Statusleiste

Die Statusleiste am unteren Terminalrand zeigt:

- **Beim Start:** `🦙 <modell_name>`
- **Während Prompt-Encoding:** `🦙 ctx: 131072 tokens | 23% (87% cached)`
- **Nach Antwort:** `🦙 <modell> | 1247 tokens (890 cached)`

### Verfügbare Tools

| Tool | Beschreibung |
|---|---|
| `bash` | Shell-Befehle ausführen |
| `read` | Dateien lesen (mit Vision-Modellen: Bilder) |
| `write` | Dateien erstellen/überschreiben |
| `edit` | Suchen und Ersetzen |
| `glob` | Dateien suchen |
| `update_plan` | Fortschritt tracken |

---

## 4. Personas und Skills

### Personas (Wer ist der Agent?)

Personas definieren die Identität des Agenten (z.B. "Senior C++ Entwickler" oder "Tech Writer"). Sie werden über Markdown-Dateien gesteuert.

#### Drei Ebenen

| Ebene | Pfad | Reichweite |
|---|---|---|
| **Global** | `~/.andy-agent/AGENTS.md` | Alle Projekte |
| **Projekt-lokal** | `personas/<name>.md` | Nur dieses Projekt |
| **Skills** | `skills/<name>/SKILL.md` | Task-spezifisch |

#### Persona-Skill-Bindung

Im `agent.sh`-Workflow ist jede Persona mit einem Skill verknüpft:

- `personas/cpp-expert.md` ↔ `skills/cpp/SKILL.md`
- `personas/python-expert.md` ↔ `skills/python/SKILL.md`
- `personas/devops.md` ↔ `skills/devops/SKILL.md`
- (usw.)

Bei der Persona-Auswahl wird automatisch der zugehörige Skill aktiviert. Pro Session ist **genau ein Skill** aktiv — keine Skill-Komposition oder -Stapelung.

#### Wie Personas funktionieren

Der Agent baut seinen System-Prompt in dieser Reihenfolge auf:

```
1. Basis-Prompt (hardcoded in tools/agent-loop.cpp)
   "You are andy-agent, a powerful local AI coding assistant..."

2. AGENTS.md-Inhalt (global oder projekt-spezifisch)
   "This project has AGENTS.md files with specific guidance..."
   [Inhalt von AGENTS.md]

3. Skills (aufgerufen bei Bedarf)
   "Skills are specialized capabilities..."
   [Inhalt der aktivierten Skill SKILL.md]

4. Environment-Kontext
   "Current working directory: ..."
   "Current date: ..."
```

#### Personas erstellen

**Globale Persona (für alle Projekte):**

```bash
cat > ~/.andy-agent/AGENTS.md << 'EOF'
# Persona: Senior C++ Engineer

## Style Guide
- Write idiomatic, modern C++ (C++17/20)
- Prefer RAII, smart pointers, and value semantics
- Use `const` correctness rigorously

## Communication
- Be concise, technical, and direct
- Explain trade-offs when making decisions
EOF
```

**Projekt-spezifische Persona:**

```bash
cat > personas/cpp-expert.md << 'EOF'
# Persona: Senior C++ Engineer

## Style Guide
- Write idiomatic, modern C++ (C++17/20)
- Prefer RAII, smart pointers, and value semantics
- Use `const` correctness rigorously
- Minimize includes, use forward declarations

## Code Quality
- Always read files before editing
- Write unit tests for new functionality
- Run `cmake --build build` after changes
EOF
```

**Task-spezifische Persona (Skill):**

```bash
mkdir -p ~/.andy-agent/skills/security-audit
cat > ~/.andy-agent/skills/security-audit/SKILL.md << 'EOF'
---
name: security-audit
description: Perform security audit on code. Use when asked to review for security issues.
---

# Security Audit Instructions

When auditing code:
1. Check for SQL injection vulnerabilities
2. Check for XSS vulnerabilities
3. Check for hardcoded secrets
4. Check for proper input validation
5. Check for authentication/authorization gaps
EOF
```

### Skills (Was kann der Agent zusätzlich?)

Skills sind wiederverwendbare Prompt-Module für spezifische, komplexe Fähigkeiten (z.B. Code-Reviews).

```bash
# Projekt-lokal
skills/<skill-name>/SKILL.md

# Global
~/.andy-agent/skills/<skill-name>/SKILL.md
```

### Tipp

1. **AGENTS.md ist der beste Ort** für Personas — projekt-spezifisch und automatisch geladen
2. **Skills** sind besser für task-spezifisches Verhalten
3. **Globale AGENTS.md** für übergreifende Standards
4. **Sei spezifisch** — "Write C++ code" ist zu vage, "Write C++17 code following Google Style Guide" ist besser
5. **Teste deine Persona** mit einfachen Tasks und passe sie an

---

## 5. Sicherheit

### Das Permission-System

Standardmäßig fragt der Agent vor jeder "gefährlichen" Aktion um Erlaubnis.

Du hast folgende Optionen:

| Option | Beschreibung |
|---|---|
| `y` (yes) | Erlaube diese Aktion einmalig |
| `n` (no) | Verweigere diese Aktion |
| `a` (always) | Erlaube für den Rest der Sitzung |
| `d` (deny) | Blockiere dauerhaft für diese Sitzung |

### YOLO-Modus (Vorsicht!)

```bash
./build/andy-agent --url http://localhost:8081 --yolo
```

**Achtung:** Im YOLO-Modus führt der Agent alle Befehle ohne Rücksprache aus!

### Sensitive Files

Das System enthält Mechanismen, um den Zugriff auf sensible Dateien (z.B. `.env`, `.ssh/id_rsa`) zu verhindern.

---

## 6. Session-Management

Sessions werden automatisch in `~/.andy-agent/sessions/` gespeichert.

### Sitzung fortsetzen

```bash
./build/andy-agent --url http://localhost:8081 --resume
```

### Explizite Session-Datei

```bash
./build/andy-agent --url http://localhost:8081 --session ./meine_session.jsonl
```

---

## 7. MCP Server

MCP (Model Context Protocol) ermöglicht die Integration externer Tools und Datenquellen (z.B. Google Search, Datenbanken, APIs).

### Konfiguration

Erstelle eine `mcp.json` im Arbeitsverzeichnis oder unter `~/.andy-agent/mcp.json`:

```json
{
  "servers": {
    "gradio": {
      "command": "npx",
      "args": ["mcp-remote", "https://example.hf.space/gradio_api/mcp/", "--transport", "streamable-http"],
      "timeout": 120000
    }
  }
}
```

### SSL-Hinweis

MCP-Server mit HTTPS (wie HuggingFace) benötigen SSL-Unterstützung:

```bash
cmake -B build -DLLAMA_BUILD_LIBRESSL=ON
cmake --build build -t andy-agent -j
```

### Tools auflisten

```bash
./build/andy-agent --url http://127.0.0.1:8080 /tools
```

---

## 8. Architektur

### Hauptkomponenten

#### 1. Der Agent Loop (`agent_loop`)

Zentrale Steuerungsinstanz:

- **Konversationsverwaltung**: Hält den Chat-Verlauf bereit
- **Inferenz-Steuerung**: Sendet die Konversation an das Inferenz-Backend
- **Tool-Orchestrierung**: Koordiniert Tool-Ausführung und fügt Ergebnisse dem Kontext hinzu
- **Kontext-Management**: Überwacht Token-Anzahl, löst Komprimierung aus
- **Sicherheitsprüfung**: Interagiert mit dem Permission-System

#### 2. Inferenz-Backend (`inference_backend`)

Abstrahiert die Kommunikation mit dem LLM. Ermöglicht den Wechsel zwischen verschiedenen Quellen (lokaler Prozess oder entfernter HTTP-Server).

#### 3. Tool-System (`tool_registry`)

Tools sind die "Hände" des Agenten. Jedes Tool ist registriert und verfügt über eine Beschreibung seiner Parameter (JSON-Schema).

#### 4. Permission-System

Gatekeeper für Sicherheit:

- **Klassifizierung**: Tools können als "gefährlich" markiert sein
- **Interaktion**: Bei kritischen Aktionen wird der Loop angehalten
- **Async-Support**: Asynchrone Bestätigungen für HTTP-API-Betrieb

#### 5. Session-Management

Verwaltet mehrere unabhängige Sitzungen mit eigenem Kontext, Status und Token-Statistiken.

### Datenfluss (Lebenszyklus einer Anfrage)

1. **Eingabe**: Benutzer sendet eine Nachricht (CLI oder HTTP)
2. **Prompt-Konstruktion**: Chat-Verlauf + Persona + Skills + Tool-Beschreibungen
3. **Inferenz**: Anfrage an das Backend senden
4. **Entscheidung**:
   - **Fall A (Antwort)**: Modell generiert finale Antwort → Loop endet
   - **Fall B (Tool-Aufruf)**: Modell fordert ein Tool an
5. **Tool-Ausführung**: Permission-System prüft → Tool ausführen → Ergebnis in Kontext schreiben
6. **Iteration**: Zurück zu Schritt 2

---

## 9. Entwickler-Guide

### Neue Tools hinzufügen

Ein Tool besteht aus drei Teilen: Ausführungslogik, Definition und Registrierung.

#### Schritt 1: Ausführungslogik

```cpp
static tool_result add_execute(const json & args, const tool_context & ctx) {
    if (!args.contains("a") || !args.contains("b")) {
        return {false, "", "Parameters 'a' and 'b' are required"};
    }

    double a = args["a"].get<double>();
    double b = args["b"].get<double>();
    double result = a + b;

    return {true, std::to_string(result), ""};
}
```

#### Schritt 2: Tool-Definition

```cpp
static tool_def add_tool = {
    "add", // Name des Tools
    "Adds two numbers together.", // Beschreibung für das LLM
    R"json({
        "type": "object",
        "properties": {
            "a": {"type": "number", "description": "First number"},
            "b": {"type": "number", "description": "Second number"}
        },
        "required": ["a", "b"]
    })json", // JSON-Schema
    add_execute // Verweis auf die Funktion
};
```

#### Schritt 3: Registrierung

```cpp
tool_registry::instance().register_tool(add_tool);
```

Tools werden im `tools/tools/`-Verzeichnis definiert und über die `tool_registry`-Singleton-Klasse registriert. Jedes Tool besteht aus einer `tool_def`-Struktur mit Name, Beschreibung, JSON-Schema und Execute-Funktion.

### Best Practices

#### Fehlerbehandlung in Tools

- Bei Fehler: `success = false`, `output = "Fehlermeldung"`
- Bei Erfolg: `success = true`, `output = "Ergebnis"`

#### Thread-Sicherheit

Da der Agent asynchron arbeitet (jede Session in eigenem Kontext), müssen globale Ressourcen thread-sicher sein.

#### Kontext-Management

Tools sollten nicht zu massive Mengen an Text zurückgeben. Nutze `limit` und `offset` (wie im `read`-Tool implementiert).

### Fortgeschritten: Eigener System-Prompt

Der Basis-System-Prompt ist hardcoded in `tools/agent-loop.cpp` (ca. Zeile 38). Er definiert die Grundrolle des Agenten und die Tool-Dokumentation.

**Hinweis:** Der System-Prompt wird dynamisch erweitert:
1. Basis-Prompt (hardcoded in `agent-loop.cpp`)
2. AGENTS.md-Inhalt (projekt- und projekt-spezifisch)
3. Aktiver Skill (`SKILL.md`)
4. Environment-Kontext (Arbeitsverzeichnis, Datum)

Um die Persona zu ändern, erstelle oder bearbeite eine Datei in `personas/` und wähle sie über `agent.sh` aus.

---

## 10. Fehlerbehebung

### "C++17 required"

Ihr Compiler unterstützt kein C++17. Aktualisieren Sie g++ auf Version 9 oder neuer.

### "pthread not found"

```bash
sudo apt install libc6-dev   # Ubuntu/Debian
sudo dnf install glibc-devel   # Fedora
```

### "vendor/nlohmann/json.hpp: No such file"

Die Vendor-Abhängigkeiten fehlen. Stellen Sie sicher, dass das `vendor/`-Verzeichnis vollständig ist.

### Kompilierung bricht mit OOM (Out of Memory) ab

```bash
cmake --build build -j1    # Nur ein Kern
```

### "Connection refused"

Stelle sicher, dass dein `llama.cpp` Server läuft und die URL (`--url`) korrekt ist.

### "Token limit reached"

Der Kontext ist zu voll. Nutze den `/compact` Befehl oder starte eine neue Session mit `/clear`.

### Windows: "_WIN32_WINNT" Fehler

Stellen Sie sicher, dass Visual Studio 2019 oder neuer verwendet wird.

### Tools fehlschlagen

Wenn ein Tool fehlschlägt, zeigt der Agent die Fehlermeldung an. Prüfe, ob die Pfade korrekt sind oder ob du die nötigen Berechtigungen hast.
