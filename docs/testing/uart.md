# UART 中断接收测试记录（STM32F103C8T6）

这份记录只验证一件事：**USART1 的阻塞发送和中断接收链路能正常工作**。
不验证 DMA，不验证协议解析，也不验证多串口。

## 1. 测试环境

| 项目 | 内容 |
|---|---|
| 日期 | 2026-11-14 |
| 主控 | STM32F103C8T6 核心板 |
| 串口模块 | CH340（USB-SERIAL CH340） |
| 调试器 | J-Link v9 |
| 烧录/调试 | SEGGER Ozone 3.3.2 |
| 固件 | `build-uart/firmware.elf` |
| 代码版本 | `e68cd02 feat(uart): add interrupt receive and board injection` |
| 串口参数 | COM6，115200，8N1，无流控 |

> COM 号以设备管理器实际显示为准；本次为 COM6。

## 2. 接线

### 2.1 串口

| CH340 | F103C8T6 | 说明 |
|---|---|---|
| TXD | PA10 | USART1_RX，交叉接 |
| RXD | PA9 | USART1_TX，交叉接 |
| GND | GND | 必须共地 |
| VCC | 不接 | 板子由 USB 供电 |

### 2.2 调试器

| J-Link | F103C8T6 | 说明 |
|---|---|---|
| GND | GND | 必须共地 |
| SWDIO | PA13 | SWD 数据 |
| SWCLK | PA14 | SWD 时钟 |
| 3.3V | 不接 | 板子由 USB 供电，避免双电源 |
| RST | NRST | 可选，连不上时接 |

### 2.3 启动与供电

| 项目 | 设置 |
|---|---|
| BOOT0 | GND（0，从 Flash 启动） |
| BOOT1 | GND（0） |
| 供电 | 板子 USB 口 |

## 3. 测试步骤

1. 编译 `build-uart`：

   ```powershell
   cmake --build build-uart
   ```

2. 用 Ozone 打开 `build-uart/firmware.elf`，Device 选 `STM32F103C8`，
   SWD 1000 kHz，执行 `Download & Reset`。

3. 打开串口监视器，选择 CH340 对应的 COM 口，设置为 115200、8N1、无流控。

4. 按一次板子复位，确认启动 banner 和初始计数。

5. 在串口发送框里发送一个字节 `0x03`（十六进制模式）。

6. 等待最多 1 秒（程序每 1 秒打印一次），观察 `rx count` 和 `last`。

## 4. 实测输出

```text
uart test: interrupt rx ready
rx count=0 last=0x00
rx count=1 last=0x03
```

说明：

- `0x03` 是 ETX（Ctrl+C）控制字符，不可打印，所以以十六进制显示是正常的。
- `count=1` 表示本次只收到 1 个字节，串口工具没有自动追加回车换行。
- `last=0x03` 表示最后收到的字节与发送字节一致。

## 5. 结果判定

| 检查项 | 通过标准 | 实测 | 结论 |
|---|---|---|---|
| 串口发送 | 复位后能看到 banner | 看到 `uart test: interrupt rx ready` | 通过 |
| 初始计数 | `rx count=0 last=0x00` | 与预期一致 | 通过 |
| 中断接收 | 发送 1 字节后 count 递增 | `rx count=1` | 通过 |
| 数据一致性 | `last` 等于发送字节 | `last=0x03` | 通过 |
| DMA | 不在本次范围 | 未测试 | 未覆盖 |

## 6. 结论

本次测试确认：

- USART1 的 PA9/PA10 引脚配置正确；
- CH340 的 TX/RX 交叉和共地正确；
- `HAL_UART_Receive_IT` 能进入中断；
- `HAL_UART_RxCpltCallback` → `HalUart::on_rx_complete` → 用户回调链路正常；
- 接收完成后能自动重新挂载中断接收。

因此，当前 UART 已支持：

- **阻塞发送**；
- **中断接收**。

DMA 发送/接收仍未实现，`UartMode::Dma` 相关接口仍返回 `NotSupported`，文档中不能写成“三种模式全部支持”。

## 7. 已知限制

1. 当前是单字节中断接收，115200 下约 11.5k 次中断/秒；功能可用，但 CPU 开销偏大，后续改 DMA + IDLE。
2. `HalUart::active_` 是单个静态指针，只支持一个 UART 实例；F407 多串口时需要“句柄 → 实例”映射。
3. `board_init()` 先用 Blocking 初始化，`app/uart_test.cpp` 又用 Interrupt 初始化一次；能工作，但属于重复初始化，后续应改为由应用决定模式。
4. 本次只验证链路，不验证协议、粘包、校验和错误处理。


