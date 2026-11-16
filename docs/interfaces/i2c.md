# I2C 接口说明

## 分层位置

```text
app/imu_test.cpp
      ↓ 只用 bsp::II2c
bsp/interface/i2c/i2c.h
      ↓ 纯虚接口，不包含 HAL
bsp/stm32/i2c/hal_i2c.cpp
      ↓ 唯一接触 I2C_HandleTypeDef 的适配层
STM32F1 HAL I2C
```

`board/f103` 负责：

- 打开 I2C1、GPIOB 时钟；
- 配置 PB6=SCL、PB7=SDA，复用开漏；
- 配置 `I2C1_EV_IRQn` / `I2C1_ER_IRQn`；
- 通过 `board_i2c()` 注入 `HalI2c` 实例。

应用层和 `modules/imu` 都不 include `stm32f1xx_hal.h`。

## 接口

```cpp
enum class I2cMode : std::uint8_t { Blocking, Interrupt };

struct I2cConfig {
    std::uint32_t clock_hz;   // 100000 或 400000
    I2cMode mode;
};

class II2c {
public:
    virtual Error init(const I2cConfig&) = 0;
    virtual Error write(uint8_t address, ConstByteSpan, uint32_t timeout_ms) = 0;
    virtual Error read(uint8_t address, ByteSpan, uint32_t timeout_ms) = 0;
    virtual Error memWrite(uint8_t address, uint8_t mem, ConstByteSpan, uint32_t timeout_ms) = 0;
    virtual Error memRead(uint8_t address, uint8_t mem, ByteSpan, uint32_t timeout_ms) = 0;
    virtual Error memWriteAsync(uint8_t address, uint8_t mem, ConstByteSpan,
                                I2cCallback, void* context) = 0;
    virtual Error memReadAsync(uint8_t address, uint8_t mem, ByteSpan,
                               I2cCallback, void* context) = 0;
    virtual bool busy() const noexcept = 0;
    virtual I2cMode mode() const noexcept = 0;
};
```

`address` 是 7 位地址，适配层内部左移 1 位再交给 HAL。

## 两种方式

| 方式 | API | 特点 |
|---|---|---|
| 阻塞 | `write/read/memWrite/memRead` | 调用任务被挂起直到完成或超时，简单可靠 |
| 中断 | `memWriteAsync/memReadAsync` | 启动后立即返回，完成时回调；缓冲区必须保持有效 |

当前 MPU6050 驱动主要用阻塞 `memRead`；`imu_test` 还会做一次中断读，验证异步路径。

## 错误码

| 场景 | 返回 |
|---|---|
| 未 init | `Error::NotInitialized` |
| 地址 > 0x7F、空指针、长度 > 65535 | `Error::InvalidArgument` |
| 已有异步传输在跑 | `Error::Busy` |
| HAL 超时 | `Error::Timeout` |
| NACK / 总线错误 / HAL 初始化失败 | `Error::Hardware` |
| 非 100k/400k | `Error::NotSupported` |

## 当前限制

- 只实现一个 I2C 实例（`HalI2c::active_` 单指针）。
- 中断方式一次只允许一个传输。
- 8 位寄存器地址；暂不支持 16 位寄存器地址的器件。
- 没有总线恢复（bus clear）逻辑；SDA 被从机拉死时需要复位或手动清总线。
