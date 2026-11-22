# UART 接口说明

## 1. 头文件与注入

- 接口：`bsp/interface/uart/uart.h`
- 板级注入：`board/f103/inc/board_uart.h`
- STM32 适配器：`bsp/stm32/uart/hal_uart.h`（只允许 board 包含）

应用代码只拿 `bsp::IUart&`：

```cpp
#include "board.h"
#include "board_uart.h"

board_init();
bsp::IUart& uart = board_uart();

const char msg[] = "hello\r\n";
const common::Error err = uart.write(
    {reinterpret_cast<const std::uint8_t*>(msg), sizeof(msg) - 1U}, 100U);
if (err != common::Error::Ok) {
    board_led_write(1);  // 明确处理，不静默失败
}
```

`board_init()` 已经用 115200、8N1、Blocking 模式初始化 `board_uart()`；
应用要换模式时重新调用 `init`。

## 2. 发送

`write()` 在 `Blocking` 与 `Interrupt` 模式下都走 HAL 阻塞发送，保证
调用返回时数据已经交给外设；`timeout_ms` 是 HAL 的阻塞超时。

## 3. 接收

### 阻塞

```cpp
std::uint8_t buffer[64];
const common::Error err = uart.read({buffer, sizeof(buffer)}, 100U);
```

`read()` 只在 `Blocking` 模式可用；`Interrupt` 模式返回 `NotSupported`，
因为接收由中断回调接管。

### 中断

```cpp
void on_byte(void*, bsp::ConstByteSpan data) {
    // 在 USART1 中断上下文执行，必须短小；不能 printf/阻塞。
    if (!data.empty()) {
        // push data.data[0] 到环形缓冲/协议解析器
    }
}

if (uart.init({115200U, bsp::UartMode::Interrupt}) != common::Error::Ok) {
    Error_Handler();
}
if (uart.startReceive(on_byte, nullptr) != common::Error::Ok) {
    Error_Handler();
}
```

当前实现是“每收 1 字节进一次中断”：

- `startReceive` 调 `HAL_UART_Receive_IT(..., 1)`；
- `HAL_UART_RxCpltCallback` 把单字节 `ConstByteSpan` 交给回调，然后自动
  重新挂接收；
- 回调里调用 `stopReceive()` 可以停止自动重挂；
- 重复 `startReceive` 返回 `Busy`；
- 回调运行在 ISR 上下文，不能调用阻塞 API。

每字节中断在 115200 下约 11.5k 次/秒，当前用于验证链路；后续协议模块
会在回调里写入环形缓冲，再在任务里解析，避免在 ISR 做重活。

## 4. 未提供 DMA

当前接口只暴露 `Blocking` 与 `Interrupt` 两种模式，不保留 DMA 占位枚举。
DMA 需要调用方保证缓冲区生命周期，并补 DMA1 Channel4/5、NVIC 和完成回调；
在实现和测试之前不写进接口，避免“声明了但返回 NotSupported”的空承诺。

## 5. 模式矩阵

| 模式 | 发送 | 接收 | 适用场景 |
|---|---|---|---|
| `Blocking` | HAL 阻塞 | HAL 阻塞 | 初始化打印、短命令、调试 |
| `Interrupt` | HAL 阻塞 | 单字节中断 + 回调 | 不定长协议、低延迟命令 |

## 6. 测试镜像

```powershell
cmake -S . -B build-uart -G Ninja -DAPP=uart_test `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake
cmake --build build-uart
```

烧录 `build-uart/firmware.elf` 后，串口每 1 秒输出：

```text
uart test: interrupt rx ready
rx count=0 last=0x00
```

用串口助手向板子发送任意字节，`rx count` 应递增，`last` 显示最后
一个字节。该测试只验证中断接收链路，不验证协议。
