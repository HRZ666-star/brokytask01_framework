# CAN Loopback 测试记录

## 目的

在不接 TJA1050、没有第二节点的情况下，验证 STM32F1 CAN 控制器和 `ICan`/`HalCan` 封装：

- 500 kbit/s 初始化；
- 标准帧发送；
- 轮询接收；
- 中断接收；
- 过滤器全收；
- 错误码返回。

## 构建

```powershell
cmake -S . -B build-can -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake `
  -DAPP=can_test -DBOARD=f103
cmake --build build-can
```

本机构建结果（2026-11-14）：

```text
text    data    bss     dec     hex
17392   120     2244    19756   4d2c
```

普通 `build` 也通过：

```text
text    data    bss     dec     hex
14980   108     2224    17312   43a0
```

分层检查：

```text
layering OK: app/modules/bsp-interface contain no HAL headers
```

## 接线

Loopback 模式不需要 TJA1050。只需要：

- 板子 USB 供电；
- J-Link：GND、SWDIO→PA13、SWCLK→PA14；
- CH340：TXD→PA10、RXD→PA9、GND。

TJA1050 到货后，外部总线再接：

```text
TJA1050 VCC → 5V
TJA1050 GND → G
TJA1050 CTX → PB9
TJA1050 CRX → PB8
CANH/CANL → 总线，两端 120 Ω
```

## 烧录

Ozone：

- Device：`STM32F103C8`
- SWD 1000 kHz
- Firmware：`build-can/firmware.elf`
- `Download & Reset`

串口：CH340 的 COM 口，115200 8N1。

## 预期输出

```text
can test: loopback 500 kbit/s
can poll rx: id=0x123 dlc=8 data=1011121314151617
can irq  rx: id=0x124 dlc=8 data=2021222324252627
can poll: id=0x200 dlc=8 data=0001020304050600
can poll: id=0x201 dlc=8 data=0102030405060701
...
```

判定：

- `can poll rx` 的 ID 为 `0x123`，数据为 `10..17`；
- `can irq rx` 的 ID 为 `0x124`，数据为 `20..27`；
- 之后每秒一组 `can poll`，ID 在 `0x200..0x20F` 循环；
- 没有 `init failed`、`send failed`、`timeout`。

## 实测状态

2026-11-14，F103C8T6，`build-can/firmware.elf`，COM6 / 115200 8N1，J-Link 复位后抓取：

```text
can test: loopback 500 kbit/s
can poll rx: id=0x123 dlc=8 data=1011121314151617
can irq  rx: id=0x124 dlc=8 data=2021222324252627
can poll: id=0x200 dlc=8 data=0001020304050600
can poll: id=0x201 dlc=8 data=0102030405060701
can poll: id=0x202 dlc=8 data=0203040506070802
```

结论：

- 500 kbit/s 初始化：通过；
- 标准帧发送：通过；
- 轮询接收：通过，ID/数据与发送一致；
- 中断接收：通过，ID `0x124`，数据 `20..27`；
- 过滤器全收：通过；
- 连续每秒收发：通过，ID 在 `0x200..0x20F` 循环。

外部 Normal 总线测试待 TJA1050 到货。

## 已知限制

- 只支持 500 kbit/s。
- 只支持一个 CAN 实例。
- 中断接收只用 FIFO0。
- Loopback 不能替代真实总线测试；外部模式还需要 TJA1050、120 Ω 和第二节点。
