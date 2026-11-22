#include "board.h"
#include "board_i2c.h"
#include "modules/imu/mpu6050.h"

#include <cstdio>

namespace {

volatile bool g_async_done = false;
common::Error g_async_result = common::Error::Unknown;
std::uint8_t g_async_raw[14] = {};

void on_async(void*, common::Error result) {
    g_async_result = result;
    __DMB();
    g_async_done = true;
}

long to_milli(float value) {
    return static_cast<long>(value * 1000.0F);
}

const char* chip_name(std::uint8_t id) {
    switch (id) {
        case modules::Mpu6050::kWhoAmIMpu6050:
            return "MPU6050";
        case modules::Mpu6050::kWhoAmIMpu6500:
            return "MPU6500";
        case modules::Mpu6050::kWhoAmIMpu9250:
            return "MPU9250";
        default:
            return "unknown";
    }
}

void print_data(const modules::Mpu6050Data& data) {
    std::printf(
        "mpu accel=%ld,%ld,%ld mg gyro=%ld,%ld,%ld mdps temp=%ld mC\r\n",
        to_milli(data.accel_g[0]), to_milli(data.accel_g[1]),
        to_milli(data.accel_g[2]), to_milli(data.gyro_dps[0]),
        to_milli(data.gyro_dps[1]), to_milli(data.gyro_dps[2]),
        to_milli(data.temperature_c));
}

}  // namespace

int main() {
    BOARD_INIT();

    bsp::II2c& i2c = board_i2c();
    if (i2c.init({400000U, bsp::I2cMode::Blocking}) != common::Error::Ok) {
        std::printf("i2c init failed\r\n");
        Error_Handler();
    }

    modules::Mpu6050 imu;
    std::uint8_t who = 0U;
    common::Error error = imu.probe(i2c, who);
    if (error != common::Error::Ok) {
        std::printf("mpu probe failed: %s\r\n", common::to_string(error));
        Error_Handler();
    }
    std::printf("mpu6050 test: WHO_AM_I=0x%02X (%s)\r\n",
                static_cast<unsigned>(who), chip_name(who));
    if (!modules::Mpu6050::isSupportedId(who)) {
        std::printf("mpu6050 test: unsupported id\r\n");
        Error_Handler();
    }

    error = imu.init(i2c);
    if (error != common::Error::Ok) {
        std::printf("mpu init failed: %s\r\n", common::to_string(error));
        Error_Handler();
    }
    std::printf("mpu6050 test: init ok\r\n");
    HAL_Delay(100U);

    g_async_done = false;
    error = i2c.memReadAsync(
        modules::Mpu6050::kAddress, 0x3BU, {g_async_raw, sizeof(g_async_raw)},
        on_async, nullptr);
    if (error != common::Error::Ok) {
        std::printf("mpu async start failed: %s\r\n", common::to_string(error));
    } else {
        for (std::uint32_t i = 0U; i < 100U && !g_async_done; ++i) {
            HAL_Delay(1U);
        }
        __DMB();
        if (g_async_done && g_async_result == common::Error::Ok) {
            std::printf("mpu async read: ok data=%02X%02X%02X%02X%02X%02X\r\n",
                        static_cast<unsigned>(g_async_raw[0]),
                        static_cast<unsigned>(g_async_raw[1]),
                        static_cast<unsigned>(g_async_raw[2]),
                        static_cast<unsigned>(g_async_raw[3]),
                        static_cast<unsigned>(g_async_raw[4]),
                        static_cast<unsigned>(g_async_raw[5]));
        } else {
            std::printf("mpu async read: failed\r\n");
        }
    }

    for (;;) {
        modules::Mpu6050Data data = {};
        error = imu.read(data);
        if (error == common::Error::Ok) {
            print_data(data);
        } else {
            std::printf("mpu read failed: %s\r\n", common::to_string(error));
        }
        HAL_Delay(500U);
    }
}
