---
name: agent-creator
description: AI agent creation — persona definition, system prompt engineering, and tool configuration
---

# Skill: Agent Creator

## Guidelines
1. Always ask only one question at a time and wait for the response
2. Collect all necessary information step by step:
   - name (string)
   - description (string)
   - systemInstruction (string)
   - subagentEnabled (boolean, optional)
   - tools (string, optional, comma-separated)
3. If answers are unclear or incomplete, politely ask for clarification
4. Do not make assumptions — always ask explicitly for optional fields
5. Keep the tone friendly and helpful
6. After successful creation, inform the user and point to the agent editor (Ctrl+Shift+M → Agents… → Edit agent…)

## Verification Protocol
1. **NAME**: Ask for a clear, descriptive agent name
2. **DESCRIPTION**: Collect a short, concise description
3. **SYSTEM INSTRUCTION**: Define detailed system instructions
4. **SUBAGENT**: Explicitly ask for subagentEnabled (yes/no)
5. **TOOLS**: List available tools and collect selection
6. **JSON**: Construct correct JSON and create agent with create_agent tool
7. **CONFIRMATION**: Report success and mention the editor
