#pragma once

#include "bsp/interface/i2c/i2c.h"

#include <cstdint>

namespace modules {

using common::Error;

struct Mpu6050Raw {
    std::int16_t accel[3] = {};
    std::int16_t gyro[3] = {};
    std::int16_t temperature = 0;
};

struct Mpu6050Data {
    float accel_g[3] = {};
    float gyro_dps[3] = {};
    float temperature_c = 0.0F;
};

/*
 * MPU6050 register-level driver.  It talks only to bsp::II2c, so the same
 * module can run on F103, F407 or a host fake without changing its code.
 *
 * Default ranges: accel +/-4 g, gyro +/-500 deg/s, DLPF 44 Hz, 200 Hz sample.
 */
class Mpu6050 final {
public:
    static constexpr std::uint8_t kAddress = 0x68U;
    static constexpr std::uint8_t kWhoAmIMpu6050 = 0x68U;
    static constexpr std::uint8_t kWhoAmIMpu6500 = 0x70U;
    static constexpr std::uint8_t kWhoAmIMpu9250 = 0x71U;

    [[nodiscard]] static bool isSupportedId(std::uint8_t id) noexcept;

    [[nodiscard]] Error probe(bsp::II2c& bus, std::uint8_t& id);
    [[nodiscard]] Error init(bsp::II2c& bus);
    [[nodiscard]] Error whoAmI(std::uint8_t& id);
    [[nodiscard]] Error readRaw(Mpu6050Raw& out);
    [[nodiscard]] Error read(Mpu6050Data& out);
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

private:
    [[nodiscard]] Error writeReg(std::uint8_t reg, std::uint8_t value);
    [[nodiscard]] Error readReg(std::uint8_t reg, std::uint8_t& value);

    bsp::II2c* bus_ = nullptr;
    bool initialized_ = false;
    std::uint8_t who_am_i_ = 0U;
};

}  // namespace modules
