# UART 接口说明

## 1. 头文件

- 接口：`bsp/interface/uart/uart.h`
- STM32 适配器：`bsp/stm32/uart/hal_uart.h`

上层只包含接口头文件，不包含 `stm32f1xx_hal.h`。

## 2. 配置与创建

```cpp
#include "bsp/interface/uart/uart.h"
#include "bsp/stm32/uart/hal_uart.h"

extern UART_HandleTypeDef huart1;  // board/f103 中已配置

bsp::HalUart uart1_adapter{huart1};
bsp::IUart& console = uart1_adapter;

const common::Error err =
    console.init({115200U, bsp::UartMode::Blocking});
if (err != common::Error::Ok) {
    // 初始化失败必须显式处理：点亮错误灯 / 记录日志 / 进入安全态
}
```

`HalUart::init` 会写入 8N1、无流控、16 倍过采样，并调用
`HAL_UART_Init`。若板级已经初始化过同一个句柄，重复调用会重新初始化；
后续会改为由 Board 统一创建并注入，避免重复初始化。

## 3. 发送

```cpp
const char msg[] = "hello\r\n";
const common::Error err = console.write(
    {reinterpret_cast<const std::uint8_t*>(msg), sizeof(msg) - 1U}, 100U);
if (err != common::Error::Ok) {
    // 超时/忙/硬件错误都要有明确动作
}
```

`timeout_ms` 为 HAL 阻塞超时，单位 ms。返回 `Timeout` 表示在超时内没有
发送完；调用方应决定重试、丢弃还是标记链路离线。

## 4. 接收

```cpp
std::uint8_t buffer[64];
const common::Error err = console.read({buffer, sizeof(buffer)}, 100U);
```

当前只有阻塞读。中断/DMA 接收通过 `startReceive(callback, context)` 注册；
在 Day 3 实现前返回 `NotSupported`，调用方必须处理该返回值，不能假设
回调一定被调用。

## 5. 模式选择

| 模式 | 适用场景 | 当前状态 |
|---|---|---|
| `Blocking` | 初始化打印、短命令、调试 | 已实现 |
| `Interrupt` | 不定长协议帧、低 CPU 占用 | Day 3 |
| `Dma` | 高频/大块数据、遥测 | Day 3 |

同一 `IUart` 实例的模式在 `init` 时确定；切换模式需要重新 `init`。
