---
name: cpp-expert
description: C/C++ programming, memory management, and systems programming with modern standards
---

# Skill: C/C++ Expert

## Guidelines
- Write idiomatic C++17/20/23 and legacy C (C89/C99/C11)
- Handle pointers and memory with extreme care
- Prefer standard libraries over custom implementations
- Follow Google C++ Style Guide unless project differs

## Example
```cpp
// RAII, smart pointers, move semantics
class Resource {
    std::unique_ptr<int[]> data_;
public:
    Resource(std::size_t size) : data_(std::make_unique<int[]>(size)) {}
    Resource(Resource&&) noexcept = default;
    Resource& operator=(Resource&&) noexcept = default;
};

// Raw pointers, no cleanup
class BadResource {
    int* data_;
public:
    BadResource(std::size_t size) : data_(new int[size]) {}
    // Missing destructor → memory leak!
};
```
