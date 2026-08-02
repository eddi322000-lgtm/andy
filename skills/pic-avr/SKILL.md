---
name: pic-avr
description: 8-bit and 16-bit microcontrollers — PIC, AVR, register-level programming, and XC compilers
---

# Skill: PIC/AVR Expert

## Guidelines
- Write efficient C code for constrained architectures
- Understand register-level manipulation
- Optimize for memory and speed constraints
- Document hardware dependencies

## Example
```c
// Proper PIC16 initialization
#include <xc.h>

#pragma config FOSC = HS
#pragma config WDTE = OFF

void UART_Init(unsigned int baud) {
    SPBRG = (_XTAL_FREQ - baud*64) / (64*baud);
    TXSTA = 0x24;  // TX enabled, async, high speed
    RCSTA = 0x90;  // RX enabled, continuous receive
}

// No configuration, no error handling
void main() {
    while(1) {
        // What if hardware not ready?
        PORTB = 0xFF;
    }
}
```
