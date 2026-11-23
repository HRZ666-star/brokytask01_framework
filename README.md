# brokytask01_framework

基于 STM32 的嵌入式框架，按 **BSP / Module / Application** 分层，使用现代 C++（C++17）和 FreeRTOS，用于任务 01（Frameworks）。

> **第一次看，先看第 1 节“当前状态”，再按第 8 节文档索引选读。**

> **我的开发方式**：AI 帮我出初稿，我负责架构约束、接口取舍、代码审查、硬件测试和文档验收。  
> 详见 [开发方式与贡献说明](docs/development/ai-assisted.md)；  
> 代码上几个有技巧的改动见 [实现笔记](docs/development/implementation-notes.md)。

## 1. 当前状态

| 能力 | 状态 | 说明 |
|---|---|---|
| 分层与依赖检查 | 完成 | `bsp/interface` 纯虚接口，`bsp/stm32` 适配，`board` 注入 |
| C++17 | 完成 | `-fno-exceptions -fno-rtti`，热路径不动态分配 |
| UART | 完成 | 阻塞发送 + 中断接收；未提供 DMA |
| CAN | 完成（Loopback） | 500 kbit/s，轮询 + 中断收；外部 TJA1050 总线未测 |
| I2C + MPU | 完成 | MPU6500（WHO_AM_I=0x70），阻塞 + 异步读 |
| PWM / Servo | PWM 完成，SG90 未通过 | PA6/TIM3_CH1 输出已用 J-Link 验证；实际舵机不转，见“已知限制” |
| FreeRTOS 六任务 | 完成 | imu / control / comm / safety / telemetry / logger |
| 命令帧 + CRC | 完成 | 合法帧改变目标，坏 CRC 计数且不改目标 |
| F407 移植 | 完成（最小） | UART + TIM2，168 MHz，串口实测通过 |
| 初始化错误返回 / 安全加固 | 完成 | `board_init()` 返回 `Error`；PWM 初始化保护；通信超时进入停机 |
| 单元测试 | 未做 | `tests/unit` 目前为空 |

## 2. 目录

```text
app/          应用入口与任务编排（main、system、各测试 APP）
modules/      设备与算法模块（imu、servo、logger、protocol）
bsp/interface 外设抽象接口（uart、can、i2c、pwm、timer、gpio）
bsp/stm32     STM32 HAL 适配层
board/f103    F103C8T6 板级初始化、中断、链接脚本
board/f407    F407VET6 最小移植
common/       Error/Result、日志记录、时间
docs/         架构、接口、移植、测试、BOM
third_party/  FreeRTOS、STM32F1/F4 HAL 与 CMSIS
tools/        分层检查、git 辅助脚本
```

## 3. 构建

依赖：

- ARM GNU Toolchain 14.3（`arm-none-eabi-gcc/g++`）
- CMake ≥ 3.20、Ninja
- STM32Cube FW_F1 1.8.7、FW_F4 1.28.0（已放入 `third_party/`）

F103 默认固件：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DBOARD=f103 -DAPP=main
cmake --build build
```

六任务系统：

```bash
cmake -S . -B build-system -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DBOARD=f103 -DAPP=system
cmake --build build-system
```

F407 最小移植：

```bash
cmake -S . -B build-f407 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DBOARD=f407 -DAPP=f407_test
cmake --build build-f407
```

`APP` 可选：

```text
main  freertos_smoke  uart_test  log_test  can_test
imu_test  imu_task  control_task  servo_test  system  f407_test
```

## 4. 烧录与串口

### F103C8T6

- J-Link：`3V→3V3`、`GND→GND`、`DIO→DIO`、`CLK→CLK`，`5V` 不接。
- CH340：`TXD→PA10`、`RXD→PA9`、`GND→GND`，`VCC` 不接。
- BOOT0 跳线接 `0`（GND），板子 USB 供电。
- 串口 115200 8N1，COM 号以设备管理器为准（本机为 COM6）。
- Ozone：Device `STM32F103C8`，SWD 1000 kHz，打开对应 `build-*/firmware.elf`，Download & Reset。

### F407VET6

- 板上 4 针 SWD：`3V/GND/DIO/CLK`；**J-Link 的 3V 必须接板子 3V3**，否则 DAP 初始化失败。
- 板上 4 针 UART：`TX→CH340 RX`、`RX→CH340 TX`、`GND→GND`。
- BOOT0/BOOT1 跳线接 GND；5V 不接。
- 预期输出：

```text
f407 port: boot, sysclk=168000000
f407 timer: 1000
f407 timer: 2000
```

## 5. 六任务系统

```text
imu       1 kHz   优先级 6   栈 256   定时器/延时唤醒，单槽队列
control   500 Hz  优先级 5   栈 384   从单槽队列取最新 IMU，PD 输出舵机
comm      事件     优先级 4   栈 384   串口中断 → StreamBuffer → 帧解析 → 命令队列
safety    10 ms   优先级 4   栈 128   事件组，监控 IMU/control/comm 超时
telemetry 20 Hz   优先级 2   栈 256   单槽队列，打印状态
logger    事件     优先级 1   栈 512   日志队列，异步写 UART
```

启动日志：

```text
system six-task demo start, imu=1kHz ctl=500Hz
free heap=...
```

详见 [docs/architecture/tasks.md](docs/architecture/tasks.md)。

## 6. 命令帧

```text
AA 55 | len | cmd | payload[len] | crc8
```

- `cmd=0x01`：设置 pitch 目标，payload 为 4 字节大端 `int32`，单位 mdeg。
- CRC8：poly `0x07`，初值 0，覆盖 `len/cmd/payload`。
- 合法帧：`AA 55 04 01 00 00 27 10 73`。
- 坏 CRC：`AA 55 04 01 00 00 27 10 00`，只增加 `cerr`，不改变目标。

详见 [docs/interfaces/protocol.md](docs/interfaces/protocol.md) 和 [docs/testing/system.md](docs/testing/system.md)。

## 7. 错误与日志

- `common::Error`：`Ok / NotInitialized / InvalidArgument / Timeout / Busy / Hardware / Checksum / Full / Empty / NotSupported / Unknown`。
- `board_init()` 返回 `common::Error`；`BOARD_INIT()` 在失败时调用 `fatal_error(error, file, line)`。
- `fatal_error()` 把错误码、文件、行号写入：

```text
g_fatal_error_code
g_fatal_file
g_fatal_line
```

然后关中断停住；Ozone/J-Link 可直接读这三个全局变量。

- 日志：`LOG_INFO("tag", "...")`，内部队列 16 条 × 96 字节，满时丢新日志并累加 `dropped`。

## 8. 文档

- [架构分层](docs/architecture/layering.md)
- [框架设计](docs/architecture/framework.md)
- [任务划分](docs/architecture/tasks.md)
- [UART](docs/interfaces/uart.md)
- [CAN](docs/interfaces/can.md)
- [I2C](docs/interfaces/i2c.md)
- [MPU6050/6500](docs/interfaces/mpu6050.md)
- [PWM](docs/interfaces/pwm.md)
- [协议](docs/interfaces/protocol.md)
- [F103→F407 移植](docs/porting/f103-to-f407.md)
- [系统测试](docs/testing/system.md)
- [BOM](docs/hardware/bom.md)
- [开发方式与贡献说明](docs/development/ai-assisted.md)
- [实现笔记：几个有技巧的改动](docs/development/implementation-notes.md)

## 9. 已知限制

- SG90 实际转动未通过：PA6 已有 50 Hz PWM（J-Link 读 TIM3 寄存器确认），但舵机无保持力/不转；代码只能写“PWM 输出验证通过”，不能写“舵机实验成功”。
- CAN 只验证了内部 Loopback；TJA1050 外部总线需要第二节点和 120 Ω 终端电阻。
- MPU 模块实际为 MPU6500（0x70），不是 MPU6050（0x68）；驱动已兼容 0x68/0x70/0x71。
- UART 没有 DMA；当前是阻塞发送 + 中断接收，满足任务 01 的“至少两种方式”。
- F407 只移植了 UART + TIM2；I2C/CAN 适配器仍是 F103-only。
- 适配器目前是单实例（`static active_`）；多 UART/I2C 需要改成 handle → instance 注册表。
- control 仍是 `vTaskDelayUntil` 驱动，不是 TIM 触发；未测 CPU 占用和最坏执行时间。
- 接口仍有预留/未使用项（如 `Result<T>`），`control_loop` 还 include 了 `comm_task.hpp`，属于已知耦合。
- 无单元测试、HIL 和 CI；`tests/unit` 为空，测试以手工记录为主。
- `fatal_error()` 不打印日志，只保存错误码/位置，适合 Ozone 调试。

## 10. 第三方

- STM32F1 HAL 1.1.10（BSD-3-Clause）
- STM32F4 HAL 1.8.2（BSD-3-Clause）
- CMSIS 5.1.0 / 5.9.0（Apache-2.0）
- FreeRTOS 10.3.1（MIT）

见 `THIRD_PARTY.md`。
