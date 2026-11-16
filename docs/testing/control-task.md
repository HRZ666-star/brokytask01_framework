# Control task 测试记录

## 1. 测试目的

验证 `control` 任务能以 500 Hz 周期消费 `imu` 任务的 1 kHz 最新快照，并输出稳定的控制量；同时记录周期抖动、队列丢帧和栈高水位。

## 2. 测试环境

| 项目 | 内容 |
|---|---|
| 主控板 | STM32F103C8T6 核心板 |
| 调试器 | J-Link v9，SWD 1000 kHz，3V 参考接板子 3V3 |
| 串口 | CH340，COM6，115200 8N1，无流控 |
| 固件 | `build-control/firmware.elf`，`BOARD=f103`，`APP=control_task` |
| 代码基线 | `4f4f1fb feat(imu): add 1 kHz imu task with period stats` 之后的 control 改动 |
| 时间基准 | TIM2，1 MHz 自由计数，低 16 位回绕 |

## 3. 接线

- J-Link：`GND→GND`、`DIO→DIO`、`CLK→CLK`、`3V→3V3`，`5V` 不接。
- CH340：`TXD→PA10`、`RXD→PA9`、`GND→GND`，`VCC` 不接。
- 板子 USB 供电；BOOT0 跳线接 `0`（GND）。

## 4. 构建与烧录

```powershell
cmake -S C:\Work\brokytask01_framework -B C:\Work\brokytask01_framework\build-control -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE:PATH=C:/Work/brokytask01_framework/cmake/toolchain-arm-none-eabi.cmake" `
  -DBOARD=f103 -DAPP=control_task
cmake --build C:\Work\brokytask01_framework\build-control
```

J-Link Commander：

```text
device STM32F103C8
si SWD
speed 1000
connect
erase
loadfile C:\Work\brokytask01_framework\build-control\firmware.elf
r
g
qc
```

## 5. 实测输出

COM6，115200 8N1，复位后抓取：

```text
[1000] [INFO] imu: seq=1000 min=992us max=14186us avg=1013us stack=214
[1000] [INFO] control: seq=500 imu=1000 dt=2026us pitch=160000 cmd=-835 sat=2 miss=0 stack=334
[2000] [INFO] imu: seq=2000 min=998us max=1002us avg=1000us stack=94
[2000] [INFO] control: seq=1000 imu=2000 dt=1999us pitch=140000 cmd=-735 sat=2 miss=0 stack=214
[3000] [INFO] imu: seq=3000 min=998us max=1002us avg=1000us stack=94
[3000] [INFO] control: seq=1500 imu=3000 dt=2000us pitch=120000 cmd=-635 sat=2 miss=0 stack=214
[4000] [INFO] imu: seq=4000 min=998us max=1002us avg=1000us stack=94
```

## 6. 结果

| 指标 | 实测值 | 判定 |
|---|---:|---|
| `imu` 稳态周期 | 1000 us | 通过 |
| `control` 稳态周期 | 1999–2000 us（约 500 Hz） | 通过 |
| `control` 栈高水位 | 214 / 384 words | 通过 |
| `control` 未取到新帧次数 | 0 | 通过 |
| 控制量饱和次数 | 2 / 500 帧 | 可接受 |
| 固件尺寸 | text 28140, data 112, bss 12956 | 已记录 |

第一个统计窗口的 `dt=2026us` 和 `imu max=14186us` 是调度器启动、首帧建立和日志初始化造成的瞬态；第二个窗口开始稳定。

## 7. 已知限制

- 当前没有电机/舵机，`cmd` 只是合成执行器指令，用于验证任务链路和定点控制运算，不代表真实闭环响应。
- `pitch` 是合成数据，MPU6050 尚未焊接到板子上；接入真实传感器后只需替换 `imu_source.cpp` 的数据源，队列和任务接口不变。
- 控制律为 pitch 单环 PD，目标固定为 0°；尚未加入角速度环、死区/回差补偿和输出限幅后的抗饱和处理。
- 周期测量分辨率 1 us；TIM2 低 16 位每 65.536 ms 回绕，任务按 16 位无符号差值处理。
