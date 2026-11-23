# FreeRTOS 任务划分

> **通俗说**：`imu` 是“眼睛”，`control` 是“大脑”，`comm` 是“耳朵”，`safety` 是“保安”，`telemetry` 是“仪表盘”，`logger` 是“记录员”。  
> 眼睛和大脑优先级最高，记录员最低；记录员来不及就丢日志，不能拖慢大脑。

## 1. 任务表

| 任务 | 优先级 | 周期/触发 | 栈（words） | 主要通信 | 职责 |
|---|---:|---|---:|---|---|
| `imu` | 6 | 1 ms（1 kHz） | 256 | 单槽队列 `xQueueOverwrite` | 生成/读取 IMU 快照，记录周期抖动 |
| `control` | 5 | 2 ms（500 Hz） | 384 | 单槽队列取最新 IMU；命令队列；事件组 | PD 计算、舵机输出、遥测快照 |
| `comm` | 4 | 事件驱动 | 384 | StreamBuffer + 命令队列 | 串口字节流 → 帧解析 → 命令 |
| `safety` | 4 | 10 ms | 128 | 事件组 | 监控 IMU/control/comm 超时，置故障位 |
| `telemetry` | 2 | 50 ms（20 Hz） | 256 | 单槽队列 | 打印姿态/目标/命令/故障/计数 |
| `logger` | 1 | 事件驱动 | 512 | 日志队列 | 异步格式化并写 UART |

> `logger` 的 512 words 来自 `modules/logger/logger.h` 的 `kTaskStackWords`；早期设计表里的 128 是笔误。

## 2. 为什么这样划分

### imu：最高优先级、1 kHz

- 姿态数据的时基必须稳定，否则 control 的微分项会抖。
- 1 kHz 是设计上限，不是题目硬性要求；任务 01 只要求周期稳定。
- 用 `vTaskDelayUntil` 而不是 `vTaskDelay`，避免累计漂移。
- 单槽队列 + `xQueueOverwrite`：control 只关心“最新一帧”，旧帧没有保留价值。

### control：500 Hz、次高优先级

- 控制周期由 2 ms 任务周期保证；当前没有额外定时器触发，靠 `vTaskDelayUntil` 维持。
- 500 Hz 是当前选择：1 kHz 对 F103 的 CPU 占用偏高，先保稳定，再考虑升到 1 kHz。
- 只读最新 IMU，不排队；控制延迟不随队列长度增长。
- 命令队列深度 4：允许短时间连发，但不会无限堆积。

### comm：事件驱动、与 safety 同优先级

- 串口中断只把字节丢进 128 字节 StreamBuffer，ISR 不做解析。
- `comm` 任务阻塞在 StreamBuffer 上，有数据才唤醒；不用轮询。
- 解析使用 `FrameParser`，CRC 错返回 `Error::Checksum`，计入 `cerr`。
- 命令通过深度 4 的队列交给 control，comm 不直接碰舵机。

### safety：10 ms、独立任务

- 安全监控不能依赖 control 自己检查自己；独立任务更容易发现 control 卡死。
- 200 ms 启动宽限，避免上电初始化阶段误报。
- 超时阈值：IMU 20 ms、control 50 ms、comm 500 ms。
- 故障位只置位、不清除；需要复位恢复，避免“故障闪一下就继续跑”。

### telemetry：20 Hz、低优先级

- 人眼看波形/日志不需要 500 Hz；20 Hz 足够。
- 低优先级，CPU 忙时让给控制和安全。
- 单槽队列覆盖旧值，串口慢时不积压。

### logger：最低优先级、事件驱动

- 日志不能阻塞控制；`LOG_*` 只入队，格式化在 logger 任务里做。
- 队列 16 条 × 96 字节，满时丢新日志并累加 `dropped`；ISR 不格式化。
- 最低优先级保证控制/安全先跑；但长时间高负载会丢日志，这是有意的取舍。

## 3. 队列/事件/缓冲

| 对象 | 深度/大小 | 生产者 | 消费者 | 满/空行为 |
|---|---|---|---|---|
| `g_imu_queue` | 1 帧 | imu | control | `xQueueOverwrite` 覆盖旧值 |
| `g_command_queue` | 4 条 | comm | control | 满则 `command_drops++` |
| `g_telemetry_queue` | 1 帧 | control | telemetry | `xQueueOverwrite` 覆盖 |
| `rx_stream` | 128 字节 | UART ISR | comm | 满则丢字节（当前未计数） |
| `g_safety_events` | 事件组 | safety | control/telemetry | 只置位，不清除 |
| `g_log_queue` | 16 条 × 96 字节 | 任意任务 | logger | 满则丢新日志并计数 |

## 4. 启动顺序

```text
board_init / BOARD_INIT
  -> uart.init(115200, Interrupt)
  -> logger.init(uart)
  -> imu_source_start(6, 256)
  -> 创建 command_queue(4)、telemetry_queue(1)、safety_events、rx_stream(128)
  -> uart.startReceive(comm_uart_rx_isr, rx_stream)
  -> control_loop_start(...)
  -> comm_task_start(...)
  -> safety_task_start(...)
  -> telemetry_task_start(...)
  -> 关 SysTick 中断、清 pending
  -> vTaskStartScheduler()
```

启动前关 SysTick 是为了避开 `HAL_Init()` 已启动 SysTick、FreeRTOS 还没初始化链表的窗口；否则第一个 tick 可能访问空 `pxCurrentTCB`。

## 5. 实测数据

| 指标 | 实测 |
|---|---|
| IMU 周期 | 稳态 min/max = 1000/1000 µs；首窗口含启动瞬态 max 14124 µs |
| IMU 栈高水位 | 94 / 256 words |
| control 周期 | 2000 µs |
| control 栈高水位 | 194 / 384 words |
| telemetry 周期 | 50 ms |
| 日志洪水 | 200 条入队 12 条，丢 188 条，`write_errors=0` |
| 日志连续运行 | 244 s 无 HardFault |
| FreeRTOS heap | 14 KB；六任务 + 队列 + logger 后可启动 |

## 6. 已知取舍

- control 目前是任务周期触发，不是硬件定时器触发；任务 01 可接受，后续可改成 TIM 触发 + 任务通知。
- PD 微分项固定乘 500（按 2 ms 假设），任务抖动时会漂；后续应使用实测 `period_us`。
- comm 超时现在会进入 control 停机条件（`kSafetyCommTimeout` 已加入），但故障位仍只锁存、不清除。
- `logger` 队列满时丢新日志，不阻塞生产者；这是实时性优先的选择。
