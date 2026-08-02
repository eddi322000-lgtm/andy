
# andy-agent-next --TL;DR--

Ein lokales, LLM-basiertes Agentensystem-Framework.
Läuft vollständig offline ohne API-Abhängigkeiten oder Telemetrie.

Das System basiert auf einer klar getrennten Architektur aus Persona, Skill und Tools.

### Kernidee

Das System trennt Verhalten und Ausführung in drei Ebenen:

Persona → definiert Rolle und Fokus des Agenten
Skill → beschreibt Arbeitslogik und Analyseverfahren
Tools → stabile, hardcodierte Funktionen (Shell, File I/O, Search, Edit)

Optional kann das System über MCP externe Toolchains und Subagenten anbinden.

### Designziele

- **vollständig offline ausführbar**
- **keine API-Abhängigkeiten**
- **reproduzierbares Verhalten pro Session**
- **minimierter System-Context für VRAM-effiziente Ausführung**
- **klare Trennung zwischen Identität (Persona) und Logik (Skill)**
- **sichere Tool-Ausführung mit Permission-System**

### Wichtige Systemeigenschaft

Skills werden session-basiert geladen und überschrieben.
Pro Session ist genau ein aktiver Skill gültig.

Es findet keine Skill-Composition oder Skill-Stapelung statt.

### Projektstruktur

andy-agent/
├── personas/        # Persona Definitionen (Role Layer)
├── skills/          # Execution Logic (Skill Layer)
├── tools/           # Hardcoded runtime tools
├── mcp/             # Optional external tool integration
├── docs/            # Architecture & developer documentation
├── build/           # compiled binary
└── agent.sh         # runtime launcher

### Schnellstart

#### 1. Build

```bash
# ins Programmverzeichnis wechseln
cd andy-agent

# CMake + Make (empfohlen)
mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

Die Binary liegt in `build/andy-agent`.

#### 2. Starten

```bash
# Interaktiv mit Server-Abfrage und Persona-Auswahl
./agent.sh

# Direkt
./build/andy-agent --url http://localhost:8081
```

### Slash-Commands

| Befehl     | Beschreibung                 |
| ---------- | ---------------------------- |
| `/exit`    | Beenden                      |
| `/clear`   | Chat-Verlauf löschen         |
| `/tools`   | Verfügbare Tools anzeigen    |
| `/skills`  | Verfügbare Skills anzeigen   |
| `/agents`  | Verfügbare Personas anzeigen |
| `/stats`   | Token-Statistiken            |
| `/compact` | Kontext komprimieren         |

### Personas

Personas steuern das agentische Verhalten über `personas/*.md`:

```bash
personas/
├── cpp-expert.md        # Senior C++ Entwickler
├── python-expert.md     # Python Experte
├── devops.md            # DevOps Engineer
├── rust-specialist.md   # Rust Spezialist
├── tech-writer.md       # Technical Writer
└── ...                 # Weitere in personas/
```

Auswahl über `agent.sh` 

### Skills

Skills definieren die Ausführungslogik eines Agenten.

Sie enthalten z. B.:

Analyse-Workflows
Entscheidungslogik
Verifikationsprozesse
Output-Struktur
Test-Generierung

Wichtige Eigenschaft:

Pro Session ist genau ein Skill aktiv.


### Tools

| Tool | Beschreibung |
|---|---|
| `bash` | Shell-Befehle ausführen |
| `read` | Dateien lesen (Bilder bei Vision-Modellen) |
| `write` | Dateien erstellen/überschreiben |
| `edit` | Suchen und Ersetzen |
| `glob` | Dateien suchen |
| `update_plan` | Fortschritt tracken |

### Sicherheit

Der Agent fragt vor gefährlichen Aktionen nach Bestätigung (`y`/`n`/`a`/`d`).
YOLO-Modus (keine Bestätigungen):

```bash
./build/andy-agent --url http://localhost:8081 --yolo
```

### Session-Management

```bash

# Fortsetzen
./build/andy-agent --url http://localhost:8081 --resume

# Explizite Session-Datei
./build/andy-agent --url http://localhost:8081 --session ./session.jsonl

# Session deaktivieren
./build/andy-agent --url http://localhost:8081 --no-session
```

### Struktur

```
andy-agent/
├── agent.sh              # Startskript (Server + Persona)
├── personas/               # Persona-Definitionen (*.md)
│   ├── cpp-expert.md
│   ├── python-expert.md
│   ├── devops.md
│   ├── rust-specialist.md
│   ├── tech-writer.md
│   ├── cve-analyzer.md
│   ├── ui-designer.md
│   ├── unit-test-writer.md
│   └── ...               # Weitere in personas/
├── build/                # Kompilierte Binary
├── docs/                 # Vollständige Dokumentation
│   └── DOCUMENTATION.md  # Alles an einem Ort
├── mcp/                  # MCP Server Konfiguration (Beispiele)
├── skills/               # Projekt-Local Skills
│   ├── cpp-expert/
│   ├── python/
│   ├── testing/
│   └── ...               # Weitere in skills/
├── tools/                # C++ Tool-Implementierungen
│   ├── tools/            # Bash, Read, Write, Edit, Glob, Plan
│   ├── mcp/              # MCP Client & Server
│   ├── agents-md/        # AGENTS.md Discovery
│   ├── skills/           # Skills-Manager
│   └── server/           # HTTP API Server
├── vendor/               # Abhängigkeiten (cpp-httplib, json, stb)
```

### Erweiterte Optionen

```bash
# Maximale Iterationen begrenzen
./build/andy-agent --url http://localhost:8081 --max-iterations 10

# MCP, Skills oder AGENTS.md deaktivieren
./build/andy-agent --url http://localhost:8081 --no-mcp
./build/andy-agent --url http://localhost:8081 --no-skills
./build/andy-agent --url http://localhost:8081 --no-agents-md

# Kontext-Komprimierung deaktivieren
./build/andy-agent --url http://localhost:8081 --no-compaction

# Zusätzliche Skills-Suche
./build/andy-agent --url http://localhost:8081 --skills-path /pfad/zu/skills

# Einfache Ausgabe (keine Farben, keine Formatierung)
./build/andy-agent --url http://localhost:8081 --simple-io --no-color
```

### Vollständige Dokumentation

Siehe [docs/DOCUMENTATION.md](docs/DOCUMENTATION.md) für Build-Anleitung, Architektur, Entwickler-Guide und alle Details.
