# System command frame and CRC validation

## 目的

验证 `system` 固件的命令帧链路：

- 合法帧能被 `comm_task` 解析并送入 `control`；
- CRC 错误帧被明确拒绝，不改变控制目标；
- 错误计数通过 telemetry 暴露，不是静默丢帧。

## 环境

| 项目 | 值 |
|---|---|
| MCU | STM32F103C8T6 |
| 调试器 | J-Link V9，SWD 1000 kHz |
| 串口 | CH340，COM6，115200 8N1 |
| 固件 | `build-system/firmware.elf` |
| 构建 | `text 33436, data 128, bss 17276`（P0 加固后） |
| 日期 | 2026-11-22 |

## 帧格式

```text
0xAA 0x55 | len | cmd | payload[len] | crc8
```

- `len`：payload 长度，0..32；
- `cmd=0x01`：设置 pitch 目标，payload 为 4 字节大端 `int32`，单位 mdeg；
- CRC8：多项式 `0x07`，初值 0，覆盖 `len/cmd/payload`。

## 测试帧

合法帧，目标 10000 mdeg：

```text
AA 55 04 01 00 00 27 10 73
```

坏 CRC 帧（最后一个字节故意改错）：

```text
AA 55 04 01 00 00 27 10 00
```

## 步骤

1. J-Link 烧录 `build-system/firmware.elf`，复位运行。
2. 打开 COM6，115200 8N1。
3. 每 100 ms 发送一次合法帧，持续 800 ms。
4. 发送一次坏 CRC 帧。
5. 继续每 100 ms 发送合法帧，观察 telemetry。

## 实测输出

合法帧被接受：

```text
[5572] [INFO] comm: cmd=0x01 target=10000 mdps
[5600] [INFO] tele: ... tgt=10000 ... rx=9 err=0 cerr=0 drop=0 f=0x00
```

连续合法帧时 `rx` 按 9 字节/帧递增：

```text
rx=9, 18, 27, 36, 45, 54, 63, 72
tgt 一直为 10000
```

发送坏 CRC 后：

```text
[10000] [INFO] tele: ... tgt=10000 ... rx=81 err=0 cerr=1 drop=0 f=0x00
```

- `cerr` 从 0 变为 1；
- `tgt` 保持 10000，没有被坏帧改写；
- `drop=0`；
- 没有 HardFault / 复位。

## 结果

| 用例 | 期望 | 实测 | 结论 |
|---|---|---|---|
| 合法帧 | `tgt=10000`，`rx +9` | `tgt=10000`，`rx=9` | 通过 |
| 连续合法帧 | `rx` 按 9 递增，`cerr=0` | `rx=9..72`，`cerr=0` | 通过 |
| 坏 CRC | `tgt` 不变，`cerr +1` | `tgt=10000`，`cerr=1` | 通过 |
| 帧错误 | 不进入 control | 无目标变化 | 通过 |

## 已知限制

- `safety` 的 comm 超时是 500 ms；停止发帧后会置位 `f=0x04`，且故障锁存，需要复位清除。测试时应保持发帧间隔小于 500 ms。
- comm 超时已进入 control 的停机条件：`kSafetyCommTimeout` 置位后，目标、输出、舵机都回 0，不会继续执行最后一条命令。可用“发一帧 → 停发 1 s → 看 `tele` 的 `tgt/cmd/a` 是否归零”验证。
- telemetry 里的 `p` 来自合成 IMU 斜坡，不是真实姿态；本测试只验证命令链路。
- CRC 错误只累加 `cerr`，不做逐帧日志，避免噪声帧刷屏。
