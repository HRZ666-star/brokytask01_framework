# 串口命令协议

## 1. 帧格式

```text
+------+------+-----+-----+----------------+-------+
| 0xAA | 0x55 | LEN | CMD | PAYLOAD[LEN]   | CRC8  |
+------+------+-----+-----+----------------+-------+
```

| 字段 | 长度 | 说明 |
|---|---:|---|
| `AA 55` | 2 | 帧头 |
| `LEN` | 1 | payload 长度，0..32 |
| `CMD` | 1 | 命令字 |
| `PAYLOAD` | LEN | 命令参数，大端 |
| `CRC8` | 1 | 覆盖 `LEN/CMD/PAYLOAD`，poly `0x07`，初值 0 |

- 最大 payload 32 字节；超过按 `InvalidArgument` 丢弃。
- 字节流解析，无动态分配；可在普通任务中运行。
- CRC 错返回 `common::Error::Checksum`，不是静默丢帧。

## 2. 命令字

| CMD | 名称 | payload | 说明 |
|---:|---|---|---|
| `0x01` | `SET_PITCH_TARGET` | `int32` 大端，单位 mdeg | 目标限幅 ±30000 mdeg |

示例：目标 `10000 mdeg`（10°）：

```text
payload = 00 00 27 10
frame   = AA 55 04 01 00 00 27 10 73
```

## 3. 解析器

`modules/protocol/frame_parser.{h,cpp}`：

```cpp
common::Error feed(std::uint8_t byte, Frame& out);
```

返回值：

| 返回值 | 含义 |
|---|---|
| `Ok` | 收到完整且 CRC 正确的帧，`out` 有效 |
| `Empty` | 尚未收完，继续喂字节 |
| `Checksum` | 帧完整但 CRC 错，已重新同步 |
| `InvalidArgument` | 长度越界/字段非法，已重新同步 |

状态机：

```text
WaitA -> Wait55 -> Length -> Command -> Payload -> Crc
```

任何非法状态/CRC 错都会 `reset()`，从下一个字节重新找 `AA 55`。

## 4. comm 任务

`app/comm_task.cpp`：

- 优先级 4、栈 384 words。
- UART 中断只做 `xStreamBufferSendFromISR`，不解析。
- `comm_task` 从 128 字节 StreamBuffer 逐字节取，喂 `FrameParser`。
- 命令合法后 `Command{id, value}` 入深度 4 的队列。
- 计数：
  - `comm_rx_bytes()`：收到的字节数；
  - `comm_frame_errors()`：命令 ID/长度非法；
  - `comm_crc_errors()`：CRC 错；
  - `comm_command_drops()`：命令队列满；
  - `comm_last_rx_tick()` / `comm_has_seen()`：判活。

## 5. telemetry

`TelemetrySnapshot` 包含：

```text
seq, imu_seq, comm_rx, comm_errors, comm_crc_errors,
comm_drops, faults, pitch_mdeg, target_mdeg, command, servo_deg
```

日志示例：

```text
[6000] [INFO] tele: t=3000 imu=6000 p=60000 tgt=10000 cmd=335 a=30 rx=9 err=0 cerr=0 drop=0 f=0x00
```

## 6. 安全超时

`safety_task` 监控：

| 对象 | 超时 | 故障位 |
|---|---:|---|
| IMU | 20 ms | `0x01` |
| control | 50 ms | `0x02` |
| comm | 500 ms | `0x04` |

- 启动 200 ms 宽限。
- 故障位只置位、不清除；需要复位恢复。
- control 的停机条件包含 `kSafetyImuTimeout | kSafetyControlTimeout | kSafetyCommTimeout`；蓝牙/串口失联后目标、输出、舵机回 0，不会继续执行最后目标。
- 停发约 500 ms 后会看到：

```text
safety: fault=0x04
```

这是预期行为，不是崩溃。

## 7. 测试

合法帧：

```text
AA 55 04 01 00 00 27 10 73
```

坏 CRC（最后一字节改 00）：

```text
AA 55 04 01 00 00 27 10 00
```

实测：

```text
comm: cmd=0x01 target=10000 mdps
tele: ... tgt=10000 ... rx=9 err=0 cerr=0
tele: ... tgt=10000 ... rx=81 err=0 cerr=1
```

- 合法帧 `tgt=10000`；
- 坏 CRC `cerr=1`，`tgt` 不变；
- 无 HardFault。

## 8. 限制

- 无 ACK/重传；命令是“尽力而为”。
- 无序列号，重复帧会重复执行（当前设置目标，幂等）。
- CRC8 只能查错，不能纠错；强干扰场景建议 CRC16。
- `comm` 每个字节一次 `xStreamBufferReceive`，115200 下约 11.5k 次唤醒/秒；后续可改成块读或 DMA+IDLE。
- `xStreamBufferSendFromISR` 满时丢字节，当前没有计数。
