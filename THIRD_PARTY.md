# Third-party components

本仓库只把“芯片生态”作为第三方依赖；BSP 接口、适配层、模块和应用代码由本项目自己实现。

| 组件 | 版本 | 许可证 | 位置 | 用途 |
|---|---|---|---|---|
| STM32F1 HAL Driver | 1.1.10 | BSD-3-Clause | `third_party/stm32f1/Drivers/STM32F1xx_HAL_Driver` | STM32F1 外设底层 |
| CMSIS Core (Cortex-M3) | 5.1.0 | Apache-2.0 | `third_party/stm32f1/Drivers/CMSIS` | 内核寄存器、启动、NVIC |
| FreeRTOS Kernel | 10.3.1 | MIT | `third_party/freertos` | 任务、队列、信号量、事件组 |
| FreeRTOS GCC ARM_CM3 port | 10.3.1 | MIT | `third_party/freertos/portable/GCC/ARM_CM3` | Cortex-M3 上下文切换 |
| FreeRTOS heap_4 | 10.3.1 | MIT | `third_party/freertos/portable/heap_4.c` | 内核堆分配 |

## 来源

- STM32F1 HAL/CMSIS 来自 STM32CubeF1 1.8.7。
- FreeRTOS 来自 STM32CubeF1 1.8.7 的 `Middlewares/Third_Party/FreeRTOS/Source`。

## 使用约束

- 保留各目录内的 `LICENSE`、版权头和来源说明。
- 不修改第三方源码；需要适配时在本项目的 `bsp/stm32` 或 `board` 中完成。
- 后续若引入 CMSIS-DSP、CMSIS-NN、SEGGER RTT 等，继续在本表登记版本、许可证和来源。

## 尚未引入

- CMSIS-DSP：等做 IMU 滤波/FFT 时再加，避免当前仓库无谓增大。
- CMSIS-NN：任务 04 部署神经网络时再加。
- STM32F4 HAL/CMSIS：做 F407 移植时再加，当前只保留 `board/f407` 目录占位。
