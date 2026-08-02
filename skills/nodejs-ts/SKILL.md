---
name: nodejs-ts
description: Modern Node.js and TypeScript — event-driven architecture, strict typing, and NPM ecosystem
---

# Skill: Node.js/TypeScript

## Guidelines
- Focus on event-driven, non-blocking I/O
- Use strict TypeScript typing everywhere
- Leverage NPM ecosystem appropriately
- Ensure type safety across boundaries

## Example
```typescript
// Strict typing, proper error handling
interface User {
  id: string;
  name: string;
  email: string;
}

async function getUser(id: string): Promise<User> {
  try {
    const response = await fetch(`/api/users/${id}`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return response.json() as User;
  } catch (error) {
    console.error('Failed to fetch user:', error);
    throw error;
  }
}

// Any type, no error handling
async function getUser(id: any): any {
  return fetch(`/api/users/${id}`);  // No type checking!
}
```
