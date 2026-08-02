---
name: stm32
description: STM32 microcontrollers — HAL/LL drivers, CubeMX configuration, and embedded middleware
---

# Skill: STM32 Specialist

## Guidelines
- Use CubeMX for initial configuration
- Choose between HAL (ease) and LL (performance)
- Document clock configuration and peripheral settings
- Debug using proper hardware tools

## Example
```c
// Proper DMA configuration for UART
void UART_Init_DMA(void) {
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_BUFFER_SIZE);
}

// Blocking, no error handling
while(1) {
    HAL_UART_Receive(&huart1, &data, 1, HAL_MAX_DELAY);  // Blocks forever!
    process(data);
}
```
