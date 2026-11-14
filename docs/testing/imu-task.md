# IMU task 1 kHz bring-up

## 目的

验证六任务中的第一个任务 `imu`：

- 由 FreeRTOS 以 1 ms 周期调度；
- 通过单槽队列发布最新一帧 IMU 快照；
- 用 TIM2 的 1 MHz 自由计数器测量周期；
- 用异步日志输出周期和栈水位，不阻塞 1 ms 控制周期。

本阶段 MPU6050 尚未焊排针，任务使用合成数据；后续只替换数据源，任务结构不变。

## 环境

| 项目 | 内容 |
|---|---|
| 主控 | STM32F103C8T6 |
| 调试器 | J-Link V9，SWD 1000 kHz |
| 串口 | CH340，COM6，115200 8N1 |
| 固件 | `build-imu-task/firmware.elf` |
| APP | `imu_task` |
| RTOS | FreeRTOS 10.3.1，1 kHz tick |
| 时间基准 | TIM2，72 MHz / 72 = 1 MHz |

## 构建

```powershell
cmake -S C:\Work\brokytask01_framework `
      -B C:\Work\brokytask01_framework\build-imu-task `
      -G Ninja `
      -DCMAKE_TOOLCHAIN_FILE=C:\Work\brokytask01_framework\cmake\toolchain-arm-none-eabi.cmake `
      -DBOARD=f103 -DAPP=imu_task

cmake --build C:\Work\brokytask01_framework\build-imu-task
```

构建结果：

```text
text    data    bss     dec     hex
26768   112     12956   39836   9b9c
```

## 接线

| J-Link | F103 |
|---|---|
| `3V` | `3.3V` |
| `GD` | `GND` |
| `DIO` | SWD 4 针座的 `DIO`（PA13） |
| `CLK` | SWD 4 针座的 `CLK`（PA14） |
| `5V` | 不接 |

CH340：`TXD→PA10`、`RXD→PA9`、`GND→GND`、`VCC` 不接。F103 由 USB 供电，BOOT0/BOOT1 接 GND。

## 实测输出

```text
[1000] [INFO] imu: seq=1000 min=993us max=14124us avg=1013us stack=214
[2000] [INFO] imu: seq=2000 min=1000us max=1000us avg=1000us stack=94
[3000] [INFO] imu: seq=3000 min=1000us max=1000us avg=1000us stack=94
```

第一个统计窗口包含调度器启动和首帧建立过程，所以 `max=14124us`；从第二个窗口开始，周期稳定在 1000 us。

## 结果

| 指标 | 结果 | 判定 |
|---|---|---|
| 任务周期 | 1000 us（1 kHz） | 通过 |
| 周期抖动 | 稳态 min/max 均为 1000 us | 通过（测量分辨率 1 us） |
| 任务栈 | 256 words，高水位 94 words | 通过，剩余约 162 words |
| 队列 | 单槽覆盖写，无阻塞 | 通过 |
| 日志 | 异步队列输出，未拉长周期 | 通过 |

## 已知限制

- 周期测量使用 TIM2 低 16 位，1 us 分辨率；稳态 min/max 相同，说明抖动小于 1 us 的测量分辨率，但不代表绝对零抖动。
- 第一个统计窗口包含启动瞬态，不计入稳态指标。
- 当前数据为合成值，尚未接入 MPU6050；接入后需要重新测一遍任务执行时间和栈水位。
- 尚未测 CPU 占用率；后续用空闲计数或 DWT 在正式五任务版本中补测。
