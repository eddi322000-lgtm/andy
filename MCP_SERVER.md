# MCP Server Export Mode

andy-agent kann jetzt als **MCP-Server** exportieren - nicht nur als Client importieren.

## Architektur

```
                    ┌─────────────────────────────────────┐
                    │         andy-agent (Master)          │
                    │                                      │
                    │  ┌──────────────┐  ┌──────────────┐  │
                    │  │ MCP-Client   │  │ MCP-Server   │  │
                    │  │ (importiert) │  │ (exportiert) │  │
                    │  └──────┬───────┘  └──────┬───────┘  │
                    └─────────┼────────────────┼───────────┘
                              │                │
                    ┌─────────┴────────┐ ┌─────┴───────────┐
                    │ VM1: devops-agent│ │ VM2: cpp-agent  │
                    │ MCP-Server:      │ │ MCP-Server:     │
                    │ git, docker,     │ │ cmake, clang,   │
                    │ kubectl          │ │ gdb             │
                    └──────────────────┘ └─────────────────┘
```

## Verwendung

### 1. Beispiel-MCP-Server starten

```bash
cd build
./andy-mcp-server
```

`andy-mcp-server` baut alle Tool-Registrierungen, Permissions und PDF/Logging-Komponenten mit ein und exportiert über STDIO die Tools:
- `bash`
- `read`
- `write`
- `edit`
- `glob`
- `plan`

### 1.1 Persona + TCP-Brücke

Für libvirt/Remote-Hosts empfehlen sich die Hilfsskripte in `utils/`:

* `utils/start-mcp-server.sh [HOST] [PORT]` startet den Server und leitet STDIO über `socat` auf einen TCP-Listener (default 0.0.0.0:31234).
* `utils/start-mcp-server-with-persona.sh [HOST] [PORT]` bietet zusätzlich die Persona-Auswahl (kopiert `skills/<persona>/SKILL.md` nach `~/.andy-agent/skills`) bevor `start-mcp-server.sh` gestartet wird.

Damit kannst du den MCP-Server im gewünschten Persona-Kontext an einer festen IP-Adresse und Port verfügbar machen.

### 2. Mit MCP-Client verbinden

```bash
# initialize
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test-client","version":"1.0.0"}}}' | ./example-mcp-server

# tools/list
echo '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' | ./example-mcp-server

# tools/call
echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"bash","arguments":{"command":"ls -la"}}}' | ./example-mcp-server
```

### 3. In libvirt-VMs verwenden

Jede VM hat einen eigenen andy-agent-Server mit eigenen Tools:

```json
// ~/.andy-agent/mcp.json
{
  "servers": {
    "devops-vm": {
      "command": "ssh",
      "args": ["devops@10.0.0.2", "andy-agent", "--mcp-export"],
      "timeout": 120000
    },
    "cpp-vm": {
      "command": "ssh",
      "args": ["cpp@10.0.0.3", "andy-agent", "--mcp-export"],
      "timeout": 120000
    }
  }
}
```

## API

### initialize

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2024-11-05",
    "capabilities": {},
    "clientInfo": {
      "name": "my-client",
      "version": "1.0.0"
    }
  }
}
```

Antwort:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {
      "tools": {
        "listChanged": true
      }
    },
    "serverInfo": {
      "name": "andy-agent",
      "version": "1.0.0"
    }
  }
}
```

### tools/list

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/list",
  "params": {}
}
```

Antwort:

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "tools": [
      {
        "name": "bash",
        "description": "Execute a shell command",
        "inputSchema": {
          "type": "object",
          "properties": {
            "command": {"type": "string"},
            "timeout": {"type": "integer"}
          },
          "required": ["command"]
        }
      }
    ]
  }
}
```

### tools/call

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "bash",
    "arguments": {
      "command": "ls -la",
      "timeout": 30000
    }
  }
}
```

Antwort:

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "total 48\ndrwxr-xr-x  5 user  staff   160 Aug  2 10:00 .\ndrwxr-xr-x  3 user  staff    96 Aug  2 09:00 ..\n-rw-r--r--  1 user  staff  1234 Aug  2 10:00 README.md"
      }
    ],
    "isError": false
  }
}
```

## Sicherheit

- Tools werden nur aus der lokalen `tool_registry` exportiert
- Keine externen Befehle werden ausgeführt
- Permission-System bleibt aktiv (kann über `yolo_mode` deaktiviert werden)
- Sensitive Files werden weiterhin blockiert

## Nächste Schritte

1. **libvirt-VMs aufsetzen** - Automatisierte VM-Erstellung mit vorinstalliertem andy-agent
2. **MCP-Config erstellen** - `mcp.json` mit SSH-Verbindungen zu den VMs
3. **Permission-Isolation** - Jede VM hat eigene Permission-Regeln
4. **Session-Management** - Sessions über VM-Grenzen hinweg verwalten
