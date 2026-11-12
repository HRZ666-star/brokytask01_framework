# 分层与接口规则

## 1. 依赖方向

```text
Application (app/)
    ↓ 只能 include modules/ 与 bsp/interface/
Module (modules/)
    ↓ 只能 include bsp/interface/ 与 common/
BSP Interface (bsp/interface/)
    ↓ 只有纯虚接口与 POD，不 include HAL
BSP Adapter (bsp/stm32/)
    ↓ 唯一允许 include stm32f1xx_hal.h / stm32f4xx_hal.h 的层
HAL / CMSIS
```

规则：

1. `app/`、`modules/`、`bsp/interface/` 中出现 `stm32f1xx_hal.h` 或
   `stm32f4xx_hal.h` 即视为分层违规。
2. 业务代码只持有 `bsp::IUart` / `bsp::ICan` 等接口引用，不持有
   `UART_HandleTypeDef`。
3. 板级代码（`board/f103`）负责创建 HAL 句柄、完成时钟/引脚配置，并把
   `HalUart` 等适配器实例注入上层。
4. 适配器只做“接口语义 ↔ HAL 状态”的转换，不写业务逻辑。

## 2. CMake target

| target | 职责 | 关键依赖 |
|---|---|---|
| `common` | `Error`、`Result<T>` | 无 |
| `bsp_interface` | 纯虚外设接口（header-only） | `common` |
| `bsp_stm32` | STM32 HAL 适配器 | `bsp_interface`、`stm32f1_hal` / `stm32f4_hal` |
| `freertos` | FreeRTOS 10.3.1 内核 | 无 |
| `firmware` | 启动文件、board、app 入口 | 以上全部 |

`bsp_stm32` 对 HAL 的链接是 `PUBLIC`，但头文件不会通过
`bsp_interface` 向上传播；上层即使链接了 `bsp_stm32`，也只能看到
`IUart`。

## 3. 错误码

`common::Error` 是所有可能失败操作的统一返回码：

| 值 | 含义 | 典型场景 |
|---|---|---|
| `Ok` | 成功 | — |
| `NotInitialized` | 未初始化 | 先 write 后 init |
| `InvalidArgument` | 参数越界/空指针 | 波特率为 0、缓冲区为空但长度非 0 |
| `Timeout` | 超时 | HAL 返回 `HAL_TIMEOUT` |
| `Busy` | 设备忙 | HAL 返回 `HAL_BUSY` |
| `Hardware` | 硬件/驱动错误 | HAL 返回 `HAL_ERROR` |
| `Checksum` | 校验失败 | 协议帧 CRC 错 |
| `Full` | 队列/缓冲满 | 环形缓冲写入 |
| `Empty` | 队列/缓冲空 | 环形缓冲读取 |
| `NotSupported` | 当前模式不支持 | 未实现的 DMA/中断路径 |
| `Unknown` | 未分类 | 兜底，不应静默返回 |

`common::Result<T>` 用于需要同时返回值和错误的接口；`Result<void>`
用于只关心成功/失败的操作。所有接口方法都标记 `[[nodiscard]]`，
调用方忽略返回值时编译器会告警。

## 4. UART 接口

`bsp/interface/uart/uart.h` 定义：

- `Span<T>` / `ByteSpan` / `ConstByteSpan`：指针 + 长度，避免丢长度。
- `UartMode`：`Blocking`、`Interrupt`、`Dma`。
- `UartConfig`：波特率与模式。
- `IUart`：`init`、`write`、`read`、`startReceive`、`stopReceive`、`mode`。

当前 `HalUart` 已实现阻塞读写；`startReceive` / `stopReceive` 返回
`NotSupported`，中断与 DMA 路径在 Day 3 补齐，届时必须保持同一接口
不变。

## 5. 分层检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_layering.ps1
```

或构建目标：

```powershell
cmake --build build --target check_layering
```
