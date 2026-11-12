# FreeRTOS 烟测（Cortex-M3 / STM32F103C8T6）

这份烟测只验证一件事：**FreeRTOS 的移植层、配置、链接脚本和中断向量能一起正常工作**。
它不碰 UART 驱动封装、不碰 CAN/I2C，也不改 `app/main.cpp` 的正常 LED+UART 示例。

## 1. 它做什么

- 创建一个优先级 2、栈 128 字（512 字节）的 `blink` 任务。
- 任务每 500 ms 翻转一次 PC13，所以 LED 应该是 **1 Hz 闪**。
- 启动调度器前打印两行：

```text
freertos smoke: boot, sysclk=72000000
freertos smoke: starting scheduler
```

- 启动调度器后不再打印；如果调度器返回，说明空闲任务/定时器任务内存不足或 port 启动失败，程序会停在 `Error_Handler()`。

## 2. 编译

在仓库根目录执行：

```powershell
cmake -S . -B build-smoke -G Ninja `
  -DAPP=freertos_smoke `
  -DCMAKE_TOOLCHAIN_FILE=C:/Work/brokytask01_framework/cmake/toolchain-arm-none-eabi.cmake
cmake --build build-smoke
```

产物是：

```text
build-smoke/firmware.elf
build-smoke/firmware.hex
build-smoke/firmware.bin
```

`build/` 仍然是正常固件（`app/main.cpp`），`build-smoke/` 是烟测固件，两者互不覆盖。

## 3. 烧录

### 方式 A：Ozone（推荐，和之前点灯一样）

1. 打开 Ozone，`File -> Open`，选 `build-smoke/firmware.elf`。
2. Device 选 `STM32F103C8`，SWD，速度 1000 kHz。
3. `Target -> Download & Reset`。
4. 按 `F5` 运行。

### 方式 B：J-Link Commander

新建 `smoke.jlink`：

```text
si SWD
speed 1000
device STM32F103C8
connect
loadfile C:\Work\brokytask01_framework\build-smoke\firmware.elf
r
g
qc
```

执行：

```powershell
& 'D:\J link驱动\JLink_V794k\JLink.exe' -CommandFile .\smoke.jlink
```

## 4. 验收

| 检查项 | 通过标准 |
|---|---|
| LED | PC13 稳定 1 Hz 闪烁，不是常亮/常灭 |
| 串口 | 115200 8N1，复位后能看到两行 `freertos smoke:` |
| 稳定性 | 连续跑 10 分钟不复位、不卡死 |
| 栈 | `uxTaskGetStackHighWaterMark()` 后续再加；本次先看现象 |

串口只看到第一行、没有第二行：检查 `xTaskCreate` 是否返回失败，或串口线是否在复位瞬间断开。
两行都有但 LED 不闪：检查 `SysTick_Handler` 是否链接到了 `xPortSysTickHandler`，以及 `configTICK_RATE_HZ` 是否为 1000。
LED 闪但串口乱码：检查 CH340 的 TX/RX 是否交叉、波特率是否 115200。

## 5. 测完切回正常固件

```powershell
cmake --build build
```

然后用 Ozone 打开 `build/firmware.elf` 烧录即可。

## 6. 通过后做什么

1. 本地提交烟测代码，建议 tag `v0.2-freertos-smoke`。
2. 回到 Day 2：`bsp/interface` 的 `Error`/`Result`/`ByteSpan`/`IUart`。
3. 后面写 UART 中断/DMA 时，所有 ISR 都要使用 `FromISR` API，并且中断优先级不能高于 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`（当前是 5）。

## 7. 常见问题

| 现象 | 先查 |
|---|---|
| 编译报 `FreeRTOSConfig.h: No such file` | `freertos` target 的 include 目录是否包含 `board/f103/inc` |
| 链接报 `undefined reference to vPortSVCHandler` | `stm32f1xx_it.cpp` 是否定义了 `SVC_Handler`，且编译时带了 `USE_FREERTOS` |
| 链接报 `undefined reference to xPortSysTickHandler` | `third_party/freertos/portable/GCC/ARM_CM3/port.c` 是否加入 `freertos` target |
| 一启动就进 `HardFault_Handler` | 先看 `configPRIO_BITS`、NVIC 优先级分组、`configMAX_SYSCALL_INTERRUPT_PRIORITY` |
| `xTaskCreate` 返回失败 | `configTOTAL_HEAP_SIZE` 是否够（当前 10 KB），栈是否过大 |
