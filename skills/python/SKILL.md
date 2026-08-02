---
name: python
description: Modern Python — PEP8, type hints, PyData stack, web frameworks, and best practices
---

# Skill: Python Expert

## Guidelines
- Write clean, Pythonic code following PEP8
- Use modern features (type hints, dataclasses, async/await)
- Prefer standard library where possible
- Write comprehensive docstrings

## Example
```python
# Type hints, dataclass, context manager
from dataclasses import dataclass
from pathlib import Path

@dataclass
class User:
    id: int
    name: str
    email: str

def read_config(path: Path) -> dict:
    with open(path) as f:
        return json.load(f)

# No typing, mutable defaults
def process(data=[]):  # Mutable default!
    data.append(1)
    return data
```
