#include "modules/imu/mpu6050.h"

namespace modules {

namespace {

constexpr std::uint8_t kRegSampleRateDiv = 0x19U;
constexpr std::uint8_t kRegConfig = 0x1AU;
constexpr std::uint8_t kRegGyroConfig = 0x1BU;
constexpr std::uint8_t kRegAccelConfig = 0x1CU;
constexpr std::uint8_t kRegAccelXoutH = 0x3BU;
constexpr std::uint8_t kRegPwrMgmt1 = 0x6BU;
constexpr std::uint8_t kRegWhoAmI = 0x75U;

constexpr float kAccelLsbPerG = 8192.0F;   // +/-4 g
constexpr float kGyroLsbPerDps = 65.5F;    // +/-500 deg/s

std::int16_t be16(const std::uint8_t* p) noexcept {
    return static_cast<std::int16_t>(
        (static_cast<std::uint16_t>(p[0]) << 8U) |
        static_cast<std::uint16_t>(p[1]));
}

}  // namespace

bool Mpu6050::isSupportedId(std::uint8_t id) noexcept {
    return id == kWhoAmIMpu6050 || id == kWhoAmIMpu6500 ||
           id == kWhoAmIMpu9250;
}

Error Mpu6050::probe(bsp::II2c& bus, std::uint8_t& id) {
    bus_ = &bus;
    const Error error = readReg(kRegWhoAmI, id);
    if (error == Error::Ok) {
        who_am_i_ = id;
    }
    return error;
}

Error Mpu6050::whoAmI(std::uint8_t& id) {
    if (bus_ == nullptr) {
        return Error::NotInitialized;
    }
    return readReg(kRegWhoAmI, id);
}

Error Mpu6050::init(bsp::II2c& bus) {
    bus_ = &bus;
    initialized_ = false;

    std::uint8_t id = 0U;
    Error error = whoAmI(id);
    if (error != Error::Ok) {
        return error;
    }
    if (!isSupportedId(id)) {
        return Error::NotSupported;
    }
    who_am_i_ = id;

    error = writeReg(kRegPwrMgmt1, 0x00U);   // wake, internal 8 MHz clock
    if (error != Error::Ok) {
        return error;
    }
    error = writeReg(kRegSampleRateDiv, 0x04U);  // 1 kHz / (1 + 4) = 200 Hz
    if (error != Error::Ok) {
        return error;
    }
    error = writeReg(kRegConfig, 0x03U);          // DLPF 44 Hz
    if (error != Error::Ok) {
        return error;
    }
    error = writeReg(kRegGyroConfig, 0x08U);      // +/-500 deg/s
    if (error != Error::Ok) {
        return error;
    }
    error = writeReg(kRegAccelConfig, 0x08U);     // +/-4 g
    if (error != Error::Ok) {
        return error;
    }

    initialized_ = true;
    return Error::Ok;
}

Error Mpu6050::readRaw(Mpu6050Raw& out) {
    if (!initialized_ || bus_ == nullptr) {
        return Error::NotInitialized;
    }

    std::uint8_t raw[14] = {};
    const Error error = bus_->memRead(
        kAddress, kRegAccelXoutH, {raw, sizeof(raw)}, 100U);
    if (error != Error::Ok) {
        return error;
    }

    out.accel[0] = be16(&raw[0]);
    out.accel[1] = be16(&raw[2]);
    out.accel[2] = be16(&raw[4]);
    out.temperature = be16(&raw[6]);
    out.gyro[0] = be16(&raw[8]);
    out.gyro[1] = be16(&raw[10]);
    out.gyro[2] = be16(&raw[12]);
    return Error::Ok;
}

Error Mpu6050::read(Mpu6050Data& out) {
    Mpu6050Raw raw = {};
    const Error error = readRaw(raw);
    if (error != Error::Ok) {
        return error;
    }

    for (std::uint32_t i = 0U; i < 3U; ++i) {
        out.accel_g[i] = static_cast<float>(raw.accel[i]) / kAccelLsbPerG;
        out.gyro_dps[i] = static_cast<float>(raw.gyro[i]) / kGyroLsbPerDps;
    }
    // MPU6050 uses 340 LSB/degC with a 36.53 degC offset.  MPU6500/9250 use
    // 333.87 LSB/degC with a 21 degC offset.
    const bool mpu6050 = (who_am_i_ == kWhoAmIMpu6050);
    out.temperature_c = static_cast<float>(raw.temperature) /
                            (mpu6050 ? 340.0F : 333.87F) +
                        (mpu6050 ? 36.53F : 21.0F);
    return Error::Ok;
}

Error Mpu6050::writeReg(std::uint8_t reg, std::uint8_t value) {
    if (bus_ == nullptr) {
        return Error::NotInitialized;
    }
    return bus_->memWrite(kAddress, reg, {&value, 1U}, 100U);
}

Error Mpu6050::readReg(std::uint8_t reg, std::uint8_t& value) {
    if (bus_ == nullptr) {
        return Error::NotInitialized;
    }
    return bus_->memRead(kAddress, reg, {&value, 1U}, 100U);
}

}  // namespace modules
