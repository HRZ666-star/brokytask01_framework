# PWM 接口与舵机

## 1. 接口

```cpp
struct PwmConfig {
    std::uint32_t frequency_hz{50};
    std::uint16_t min_pulse_us{500};
    std::uint16_t max_pulse_us{2500};
    std::uint32_t timer_clock_hz{72000000};
};

class IPwm {
public:
    virtual common::Error init(const PwmConfig& config) = 0;
    virtual common::Error setPulseUs(std::uint16_t pulse_us) = 0;
    [[nodiscard]] virtual std::uint16_t pulseUs() const noexcept = 0;
    [[nodiscard]] virtual const PwmConfig& config() const noexcept = 0;
};
```

> `timer_clock_hz` 放在接口里是当前实现的妥协，属于板级细节；后续应移到 `HalPwm` 构造函数或 `board_pwm()`，接口只保留频率/脉宽范围。

## 2. STM32F103 适配

`bsp/stm32/pwm/hal_pwm.cpp`：

```text
ticks_per_us = timer_clock_hz / 1'000'000
period_us     = 1'000'000 / frequency_hz
prescaler     = ticks_per_us - 1
period_ticks  = period_us * ticks_per_us
ARR           = period_ticks - 1
```

- 50 Hz → 20,000 µs；72 MHz → 72 ticks/µs；`PSC=71`，`ARR=1,439,999` 或等价分频组合。
- `setPulseUs()` 检查 `min_pulse_us..max_pulse_us`，写 `__HAL_TIM_SET_COMPARE`。
- 初始化顺序：`HAL_TIM_PWM_Init` → `HAL_TIM_PWM_ConfigChannel` → `HAL_TIM_PWM_Start`。
- 通道配置：PWM1、高有效、`Pulse=0` 初始。

当前板级：

| 信号 | 引脚 | 定时器 | 状态 |
|---|---|---|---|
| yaw | PA6 | TIM3_CH1 | 已实现，`servo_test` 验证 |
| pitch | PA7 | TIM3_CH2 | **未实现**，接上去不会动 |

`board/f103/board.cpp` 的 `MX_TIM3_Init()`：

- 开 `TIM3`、`GPIOA` 时钟；
- PA6 配 `GPIO_MODE_AF_PP`；
- `htim3` 由 `HalPwm` 使用；
- `board_pwm()` 返回 `bsp::IPwm&`。

## 3. 舵机映射

`modules/servo/servo.cpp`：

```text
pulse = min_pulse + (angle - min_angle) * (max_pulse - min_pulse)
                         / (max_angle - min_angle)
```

默认 `500..2500 µs` ↔ `-90..+90°`，即：

| 角度 | 脉宽 |
|---:|---:|
| -90° | 500 µs |
| 0° | 1500 µs |
| +90° | 2500 µs |

SG90 实际可用范围通常小于 500–2500 µs；全行程可能顶到机械限位，导致堵转/发烫。正式使用建议先按 1000–2000 µs、±45° 空载测试，再逐步扩。

## 4. 使用示例

```cpp
#include "board_pwm.h"
#include "modules/servo/servo.h"

modules::Servo servo;

void init() {
    if (board_pwm().init({50, 500, 2500, 72000000}) != common::Error::Ok) {
        fatal_error(common::Error::Hardware, __FILE__, __LINE__);
    }
    if (servo.init(board_pwm(), {500, 2500, -90, 90}) != common::Error::Ok) {
        fatal_error(common::Error::Hardware, __FILE__, __LINE__);
    }
}

void set_pitch(int16_t deg) {
    (void)servo.setAngleDeg(deg);
}
```

## 5. 验证结果

### 已通过

J-Link 直接读 F103 寄存器（`servo_test` 运行中）：

```text
GPIOA_CRL  = 0x4B444444   // PA6 = AF 推挽
TIM3_CR1   = 0x00000081   // 计数器启动
TIM3_PSC   = 0x00000047   // 71
TIM3_ARR   = 0x00004E1F   // 19999
TIM3_CCMR1 = 0x00000068   // PWM1
TIM3_CCER  = 0x00000001   // CH1 使能
TIM3_CCR1  = 0x000002D2 / 0x00000955 / 0x000009C4  // 随扫角变化
```

串口输出：

```text
servo test: angle=-70 pulse=722us
servo test: angle=-60 pulse=833us
...
servo test: angle=0 pulse=1500us
...
servo test: angle=70 pulse=2278us
```

结论：**PA6 的 50 Hz PWM 和脉宽-角度换算已通过**。

### 未通过

SG90 实际转动未通过：

- A6 有 PWM，但舵机接上后输出轴没有保持力；
- 拔掉 A6，输出轴两边都能轻松转；
- 换信号杜邦线无反应；
- 红/棕曾反接并发烫，换正后不发烫但仍不转。

因此文档只能写“PWM 输出验证通过”，不能写“舵机实验成功”。可能原因：信号针接触不良、3.3 V 逻辑电平不足、舵机已被反接损伤。下一步优先换第二个舵机、用 3 针公排针转接，必要时加 74HCT125/三极管电平转换。

## 6. 已知限制

- 只有 TIM3_CH1（PA6）一路；PA7 第二路未实现。
- `timer_clock_hz` 在接口里，换板子要改调用点；后续移到 adapter。
- 无死区/回差补偿；SG90 塑料齿轮回差大。
- 无电流/温度保护；堵转会发烫。
- `setPulseUs()` 不做斜率限制，阶跃会给舵机最大加速度。
