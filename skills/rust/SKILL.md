---
name: rust
description: Systems programming with Rust — ownership model, safe concurrency, and Cargo ecosystem
---

# Skill: Rust Specialist

## Guidelines
- Provide idiomatic Rust code
- Focus on ownership and lifetime management
- Prefer safe abstractions over unsafe blocks
- Use Cargo-based workflows consistently

## Example
```rust
// Idiomatic Rust with proper error handling
use std::fs::File;
use std::io::{self, Read};

fn read_config(path: &str) -> Result<String, io::Error> {
    let mut file = File::open(path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;
    Ok(contents)
}

// Ignoring errors, unsafe
fn read_config_bad(path: &str) -> String {
    let file = File::open(path).unwrap();  // Panics on error!
    let mut contents = String::new();
    file.read_to_string(&mut contents).unwrap();
    contents
}
```
