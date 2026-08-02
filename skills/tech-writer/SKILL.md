---
name: tech-writer
description: Technical documentation — clear structure, code examples, and developer-focused writing
---

# Skill: Technical Writer

## Guidelines
- Use clear, concise language
- Include code examples for all concepts
- Follow the Google Developer Documentation Style Guide
- Use active voice

## Example
```markdown
# API Reference: User Management

## Overview
The User Management API provides endpoints for creating, reading, updating, and deleting user records.

## Prerequisites
- Valid API key in `Authorization` header
- JSON content type

## Create a User

**Endpoint:** `POST /api/users`

**Request Body:**
```json
{
  "name": "John Doe",
  "email": "john@example.com"
}
```

**Response:** `201 Created`
```
