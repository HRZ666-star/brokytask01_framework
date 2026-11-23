# 几个有技巧的代码改动

> 这里记录的是我在任务 01 里做的几个改动。它们**看起来小，但改变了行为，或者帮我避开了一个坑**。  
> 每条都写：我当时遇到什么、我怎么改、为什么我觉得这算技巧、证据在哪。

## 1. UART：把“空承诺”删掉，而不是假装支持 DMA

### 我当时遇到的问题

我最初 `UartMode` 有 `Blocking / Interrupt / Dma` 三种，`HalUart::write()` 在 `Dma` 模式直接返回 `NotSupported`。  
接口承诺了三种，实际只支持两种。

### 我怎么改

- `UartMode` 只保留 `Blocking`、`Interrupt`；
- 删除 `IUart::mode()` 和 `HalUart::mode()`；
- `startReceive()` 要求 `mode_ == UartMode::Interrupt`，否则返回 `NotSupported`；
- 文档同步改为“未提供 DMA”。

### 为什么我觉得这算技巧

任务 01 只要求“至少两种方式”。  
**删掉第三种空承诺，比保留一个永远返回 `NotSupported` 的枚举更诚实，也让接口和测试证据一致。**

### 证据/文件

- `bsp/interface/uart/uart.h`
- `bsp/stm32/uart/hal_uart.cpp`
- `docs/interfaces/uart.md`
- 构建：`build` / `build-system` / `build-uart` / `build-control` 全部通过

---

## 2. CRC：把 `bool` 改成 `Error`，区分“没收完”和“收错了”

### 我当时遇到的问题

`FrameParser::feed()` 我最初返回 `bool`：

- 没收完整帧 → `false`
- CRC 错 → `false`
- 长度越界 → `false`

我当时无法区分“继续等字节”和“这帧坏了”，CRC 错只能静默丢。

### 我怎么改

```cpp
common::Error feed(std::uint8_t byte, Frame& out);
```

- `Ok`：完整帧
- `Empty`：还没收完，继续喂
- `Checksum`：CRC 错
- `InvalidArgument`：长度越界

`comm_task` 在 `Checksum` 时累加 `g_crc_errors`，遥测增加 `cerr`；坏帧不改变目标。

### 为什么我觉得这算技巧

字节流解析里，“没收完”不是错误，“收错了”才是错误。  
把两者分开，解析器才能在不完整帧上继续等待，而不是一有 `false` 就 reset。

### 证据/文件

- `modules/protocol/frame_parser.h/.cpp`
- `app/comm_task.cpp`
- `app/telemetry_task.cpp`
- 实测：合法帧 `rx=9 tgt=10000 cerr=0`；坏 CRC 帧 `cerr=1`，`tgt` 仍 10000

---

## 3. 启动 HardFault：SysTick 早启动 + 栈未对齐

### 我当时遇到的问题

`HAL_Init()` 会启动 SysTick，但此时 `vTaskStartScheduler()` 还没初始化 FreeRTOS 链表。  
第一个 tick 进 `xPortSysTickHandler()`，访问空的 `pxCurrentTCB`，HardFault。

另外链接脚本 `_estack = 0x20004FFF` 不是 8 字节对齐。

### 我怎么改

1. `SysTick_Handler` 增加守卫：

```cpp
if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
    xPortSysTickHandler();
}
```

2. 启动调度器前关 tick、清 pending：

```cpp
SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
```

3. `_estack` 改为 `0x20005000`。

### 为什么我觉得这算技巧

这是我当时遇到的两个独立问题叠在一起：

- 一个是 RTOS 启动顺序；
- 一个是 ARM 栈对齐。

只修一个，另一个还会以“随机 HardFault”出现。  
守卫放在 `SysTick_Handler` 里，对所有 APP 都生效，不是只修 `freertos_smoke`。

### 证据/文件

- `board/f103/stm32f1xx_it.cpp`
- `app/freertos_smoke.cpp`
- `app/system_task.cpp`
- `board/f103/linker.ld`
- 修复后串口输出 `freertos smoke: starting scheduler`，PC13 正常翻转

---

## 4. 1 kHz 周期测量：DWT 不靠谱，换 TIM2 + 16 位回绕

### 我当时遇到的问题

我最初用 DWT `CYCCNT` 测任务周期，任务里读到的值恒定不变；J-Link 直接读却在变化。  
换 TIM2 1 MHz 后，32 位差值又出现 `0xFFFF03E8` 这种巨值——16 位计数器回绕了。

### 我怎么改

- TIM2：`Prescaler = 71`，72 MHz / 72 = 1 MHz；
- `Period = 0xFFFF`，16 位回绕；
- 时间戳用 `std::uint16_t`，差值 `now - last` 按 16 位模运算；
- `board_micros()` 返回 `__HAL_TIM_GET_COUNTER(&htim2)`。

### 为什么我觉得这算技巧

16 位计数器在 65.536 ms 回绕，`uint16_t` 的无符号减法天然是模 65536 的，  
所以 `now - last` 在回绕点仍然正确，不需要加 `if (now < last)` 分支。

### 证据/文件

- `board/f103/board.cpp` 的 `MX_TIM2_Init()`
- `board/f103/inc/board.h` 的 `board_micros()`
- `app/imu_source.cpp`
- 实测稳态：`min=1000us max=1000us avg=1000us`

---

## 5. 板级初始化：从 `void + Error_Handler` 改成 `Error + fatal_error`

### 我当时遇到的问题

`board_init()` 我最初返回 `void`，任何一步失败都进 `Error_Handler()` 死循环。  
我不知道错在哪，也没法在测试里判断。

### 我怎么改

- `board_init()` 返回 `common::Error`；
- 每个 `MX_*` / `SystemClock_Config` 也返回 `Error`；
- 失败立即返回，不再在中间层 `Error_Handler()`；
- `BOARD_INIT()` 宏在调用点失败时进 `fatal_error(error, file, line)`；
- `fatal_error` 把错误码/文件/行号写入全局，关中断停住，Ozone 可读。

### 为什么我觉得这算技巧

错误处理没有“全局 try/catch”，只能一层层返回。  
把 `fatal_error` 放在**调用点**而不是初始化内部，既保留错误码，又保留“谁调用失败”的文件/行号。

### 证据/文件

- `board/f103/inc/board.h`
- `board/f103/board.cpp`
- `board/f407/board.cpp`
- 所有 `app/*.cpp` 入口改为 `BOARD_INIT()`

---

## 6. P0 安全加固：初始化保护、ISR 可见性、通信超时停机

### 我当时遇到的问题

- `HalPwm::setPulseUs()` 在 `init()` 前也能返回 `Ok`，会写未初始化定时器；
- `receiving_` / `busy_` 被 ISR 和任务共享，没 `volatile`；
- `kSafetyCommTimeout` 置位后，control 仍只检查 IMU/control 超时，蓝牙失联不会停机。

### 我怎么改

- `HalPwm` 增加 `initialized_`，`HAL_TIM_PWM_Start` 成功后才置 true；
- `HalUart::receiving_`、`HalCan::receiving_`、`HalI2c::busy_` 改 `volatile bool`；
- `HalI2c` 阻塞传输前后维护 `busy_`；
- `kSafetyCritical` 加入 `kSafetyCommTimeout`；
- control 用 `faulted = (faults & kSafetyCritical) != 0` 统一判断。

### 为什么我觉得这算技巧

安全不是“加一个 if”，而是让**所有异常都走同一个停机条件**。  
把通信超时并入 `kSafetyCritical`，控制代码不再需要知道“这次是哪个超时”。

### 证据/文件

- `bsp/stm32/pwm/hal_pwm.h/.cpp`
- `bsp/stm32/uart/hal_uart.h`
- `bsp/stm32/can/hal_can.h`
- `bsp/stm32/i2c/hal_i2c.h/.cpp`
- `app/safety_task.hpp`
- `app/control_loop.cpp`

---

## 7. RTOS 语义：单槽队列、drop-new 日志、StreamBuffer

### 我当时遇到的问题

- 控制只要最新 IMU，队列堆积旧帧没有意义；
- 日志不能阻塞控制，队列满不能等；
- 串口 ISR 里不能做协议解析。

### 我怎么改

- IMU → 单槽队列 + `xQueueOverwrite`；
- logger → 深度 16 的队列，满则丢新日志并 `dropped++`；
- UART ISR 只把字节塞进 `StreamBuffer`，`comm_task` 再做解析。

### 为什么我觉得这算技巧

三种 IPC 分别对应三种语义：

| 场景 | 语义 | IPC |
|---|---|---|
| IMU | 只要最新 | 单槽队列覆盖 |
| 日志 | 不能阻塞 | 队列 + drop-new |
| 串口 | 字节流 | StreamBuffer |

这不是“随便选一个队列”，而是按“旧数据有没有价值”来选。

### 证据/文件

- `app/imu_source.cpp`
- `modules/logger/logger.cpp`
- `app/comm_task.cpp`

---

## 8. 一句话

> **这些改动的共同点：不是加功能，而是让“接口承诺、错误语义、启动顺序、时间基准、停机条件”彼此一致。**
