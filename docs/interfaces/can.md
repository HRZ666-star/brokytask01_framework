# CAN 接口说明

## 分层位置

```text
app/can_test.cpp
      ↓ 只用 bsp::ICan
bsp/interface/can/can.h
      ↓ 纯虚接口，不包含 HAL
bsp/stm32/can/hal_can.cpp
      ↓ 唯一接触 CAN_HandleTypeDef 的适配层
STM32F1 HAL CAN
```

`board/f103` 只负责：

- 打开 CAN1、AFIO、GPIOB 时钟；
- 把 CAN 重映射到 PB8/PB9；
- 配置 NVIC；
- 把 `HalCan` 实例通过 `board_can()` 注入给上层。

应用层不 include `stm32f1xx_hal.h`。

## 接口

```cpp
namespace bsp {

enum class CanMode : std::uint8_t { Normal, Loopback };

struct CanFrame {
    std::uint32_t id;
    bool extended;
    bool remote;
    std::uint8_t dlc;
    std::uint8_t data[8];
};

struct CanConfig {
    std::uint32_t bitrate;   // 当前只支持 500000
    CanMode mode;
};

class ICan {
public:
    virtual Error init(const CanConfig&) = 0;
    virtual Error start() = 0;
    virtual Error stop() = 0;
    virtual Error send(const CanFrame&, std::uint32_t timeout_ms) = 0;
    virtual Error receive(CanFrame&, std::uint32_t timeout_ms) = 0;
    virtual Error startReceive(CanRxCallback, void* context) = 0;
    virtual Error stopReceive() = 0;
    virtual CanMode mode() const noexcept = 0;
};

}  // namespace bsp
```

## 两种工作模式

| 模式 | 是否需要 TJA1050 | 用途 |
|---|---|---|
| `CanMode::Loopback` | 不需要 | 验证控制器、过滤器、中断、收发路径 |
| `CanMode::Normal` | 需要 | 真实 CAN 总线，必须至少有一个 ACK 节点 |

Loopback 下帧只在 STM32 CAN 控制器内部回环，PB8/PB9 不参与总线；因此可以现在先验收驱动。

## 位时序

当前板子 APB1 = 36 MHz：

```text
36 MHz / 8 = 4.5 MHz
4.5 MHz / (1 + 6 + 2) = 500 kHz
采样点 = (1 + 6) / 9 = 77.8 %
```

对应：

```cpp
Prescaler = 8;
TimeSeg1  = CAN_BS1_6TQ;
TimeSeg2  = CAN_BS2_2TQ;
SyncJumpWidth = CAN_SJW_1TQ;
```

## 错误码

| 场景 | 返回 |
|---|---|
| 未 init / 未 start | `Error::NotInitialized` |
| `dlc > 8` 或 ID 越界 | `Error::InvalidArgument` |
| 邮箱满且 timeout=0 | `Error::Busy` |
| 等邮箱/等接收超时 | `Error::Timeout` |
| 轮询接收无帧且 timeout=0 | `Error::Empty` |
| HAL 初始化/配置/收发失败 | `Error::Hardware` |
| 非 500 kbit/s | `Error::NotSupported` |

## 当前限制

- 只实现一个 CAN 实例（`HalCan::active_` 单指针），F407 双 CAN 时需要句柄到实例的映射。
- 固定 500 kbit/s，后续可把位时序计算抽成函数。
- 中断接收只使用 FIFO0。
- 尚未处理 bus-off 自动恢复策略；当前 `AutoBusOff = ENABLE`，只依赖 HAL 状态。
- 外部总线测试需要 TJA1050、120 Ω 终端和一个能 ACK 的第二节点。
