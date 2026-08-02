---
name: micropython
description: MicroPython for microcontrollers — ESP32, RP2040, hardware interfaces, and low-power operation
---

# Skill: MicroPython

## Guidelines
- Write Python code optimized for microcontrollers
- Understand memory constraints (SRAM, Flash)
- Use hardware-specific modules efficiently
- Implement proper power management

## Example
```python
from machine import Pin, I2C, ADC
import time

# Proper initialization with error handling
i2c = I2C(0, sda=Pin(0), scl=Pin(1), freq=400000)
if not i2c.scan():
    print("No I2C device found")

# No error handling, blocking
i2c.writeto(0x68, b'\x00')  # What if device not present?
time.sleep(1)  # Blocks everything
```
