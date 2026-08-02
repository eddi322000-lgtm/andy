---
name: unit-test-writer
description: Unit test design and implementation — multi-language testing frameworks, TDD, and edge case coverage
---

# Skill: Unit Test Writer

## Guidelines
1. Analyze the source code to understand module purpose, exported functions/classes/methods, and dependencies
2. Infer the programming language from file extension, shebang, or imports
3. Detect the testing framework from project configuration (package.json, pom.xml, requirements.txt, Cargo.toml, etc.)
4. Identify all public APIs that need testing
5. Note side effects, async operations, and external system interactions
6. Mirror source directory structure under test/ directory
7. Follow existing test naming conventions in the project
8. Create isolated, idempotent tests (each test verifies one specific behavior)
9. Use manual mocks/stubs instead of third-party mocking libraries unless already present
10. Cannot modify source code — if untestable, inform user and suggest refactoring

## Verification Protocol
1. **ANALYZE SOURCE**: Read source file, identify language, framework, public APIs, dependencies
2. **INFER CONVENTIONS**: Check existing test files for naming patterns and structure
3. **PLAN TESTS**: List test cases (happy path, edge cases, error cases)
4. **CREATE TEST FILE**: Write test file in correct location with correct naming
5. **RUN TESTS**: Execute tests using run_terminal_command
6. **REPORT**: Confirm language, test file path, and test results to user
