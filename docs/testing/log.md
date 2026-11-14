# 异步日志构建与硬件验证记录

## 1. 测试对象

- 板卡：STM32F103C8T6
- 固件：`build-log/firmware.elf`（`APP=log_test`）
- 串口：USART1，PA9/PA10，115200 8N1
- 日志实现：`common/log` + `modules/logger`

## 2. 构建环境

| 项目 | 值 |
|---|---|
| 工具链 | ARM GNU Toolchain 14.3.rel1 |
| CMake | 4.5.0 |
| 生成器 | Ninja |
| 构建目录 | `C:\Work\brokytask01_framework\build-log` |

构建命令：

```powershell
cmake -S . -B build-log -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake `
  -DAPP=log_test
cmake --build build-log
```

## 3. 构建结果

```text
text    data    bss     dec     hex
21372   124     12904   34400   8660
```

- 构建退出码：0
- 分层检查：`layering OK`
- 普通 `build`、`build-uart`、`build-smoke` 均可继续构建，未受日志模块影响。

## 4. 预期串口输出

```text
log test: boot, sysclk=72000000
[0] [INFO] boot: logger test start
[..] [INFO] test: heartbeat=0
[..] [WARN] test: warn at 0
[..] [ERROR] test: error at 0
[..] [INFO] test: heartbeat=1
...
[..] [INFO] flood: dropped=...
```

判定标准：

- 能看到 `INFO / WARN / ERROR` 三种级别；
- `flood` 任务连续写入 200 条后，`dropped` 大于 0；
- 串口输出不卡死，`test` 任务继续按 500 ms 周期打印；
- `write_errors` 为 0（CH340 正常时）。

## 5. 硬件实测

### 5.1 测试条件

- 板卡：STM32F103C8T6 最小系统板，USB 供电
- 调试器：J-Link v9，SWD 1000 kHz
- 串口：CH340，COM6，115200 8N1，无流控
- 固件：`build-log/firmware.elf`（`APP=log_test`）
- 烧录方式：Ozone `Download & Reset`，随后按板载 `RESET`
- 启动保护：`vTaskStartScheduler()` 前关闭 SysTick 中断并清除 pending，避免 HAL 时基在调度器初始化期间触发 tick

### 5.2 实测输出

复位后从 0 开始：

```text
log test: boot, sysclk=72000000
[0] [INFO] boot: logger test start
[0] [INFO] test: heartbeat=0
[0] [WARN] test: warn at 0
[0] [ERROR] test: error at 0
[0] [INFO] flood: message=0
...
[1] [INFO] flood: message=11
[500] [INFO] test: heartbeat=1
[1000] [INFO] test: heartbeat=2
[1500] [INFO] test: heartbeat=3
[2000] [INFO] test: heartbeat=4
[2011] [INFO] flood: dropped=188 write_errors=0
[2500] [INFO] test: heartbeat=5
[2500] [WARN] test: warn at 5
...
[20000] [INFO] test: heartbeat=40
```

### 5.3 结果

| 检查项 | 实测结果 | 判定 |
|---|---|---|
| 开机横幅 | `log test: boot, sysclk=72000000` | 通过 |
| 首次日志时间戳 | `[0]` | 通过 |
| 分级日志 | `INFO / WARN / ERROR` 均出现 | 通过 |
| `flood` 200 条 | 入队 12 条，`dropped=188` | 通过（队列满丢新日志） |
| 串口写失败 | `write_errors=0` | 通过 |
| 队列满时心跳 | `test` 仍严格 500 ms 递增 | 通过 |
| 长时间运行 | 连续运行至 244 s 无 HardFault；复位后 20 s 正常 | 通过 |

### 5.4 结论

异步日志的主链路通过：任务侧只做格式化并入队，日志任务从队列取出后经 `IUart` 输出；队列满时丢弃新日志并计数，不阻塞生产者。`flood` 的 200 条中只入队 12 条、丢弃 188 条，是 16 深度队列加低优先级日志任务的预期结果，不是串口丢数据。

## 6. 已知限制

1. 只支持任务上下文调用 `LOG_*`，暂不提供 ISR 版本；中断里不能调用 `vsnprintf`。
2. 队列深度 16，队列满时丢弃新日志并累加 `dropped`，不阻塞生产者。
3. 日志任务优先级 1，低于测试任务；115200 下一条 96 字节日志约 8 ms。
4. `LogRecord` 固定 96 字节正文、16 字节 tag，超长内容会被 `vsnprintf` 截断。
5. 当前 `common/log` 不依赖 UART，`modules/logger` 通过 `IUart` 注入输出设备，换串口不需要改日志核心。
