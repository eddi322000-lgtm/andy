---
name: embedded-systems
description: Embedded C/C++, RTOS, hardware interfacing, microcontrollers, and low-level systems programming
---

# Skill: Embedded Systems

## Guidelines
- Prioritize code size and power efficiency
- Understand hardware constraints (memory, clock speed)
- Write interrupt-safe code (critical sections, atomic operations)
- Document hardware dependencies clearly

## Example
```c
// Interrupt-safe with critical section
volatile uint8_t flag = 0;

void ISR_HANDLER(void) {
    __disable_irq();
    flag = 1;
    __enable_irq();
}

// Race condition, no synchronization
void ISR_HANDLER(void) {
    counter++;  // Not atomic!
}
```
