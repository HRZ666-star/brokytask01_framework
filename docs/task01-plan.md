# 任务 01 实施计划：从点灯到可移植框架

> 仓库：<https://github.com/HRZ666-star/brokytask01_framework>
> 主开发板：STM32F103C8T6（Cortex-M3，64 KB Flash / 20 KB RAM）
> 移植验证板：STM32F407VET6（Cortex-M4F，512 KB Flash / 192 KB RAM）
> 参考框架：[HNUYueLuRM/basic_framework](https://github.com/HNUYueLuRM/basic_framework)

---

## 0. 目标与验收

题目五条要求，最终都要落到可检查的证据上：

| 要求 | 本计划的落点 | 证据 |
|---|---|---|
| 封装 UART/USART、CAN、I2C | `bsp/interface` + `bsp/stm32` | 接口文件、三模式测试、波形 |
| 封装所购物资 | `modules/`：MPU6050、TJA1050、SG90、编码器、按键、LED | 单模块测试程序 |
| 模块化、高扩展性 | BSP / Module / App 三层 + 依赖规则 | 目录树、CMake target、分层检查脚本 |
| C++17、业务不碰 HAL | 接口层纯抽象，HAL 只在 `bsp/stm32` | CI grep、F407 移植零修改 |
| FreeRTOS 任务划分 | 6 个任务、优先级/周期/栈/IPC 表 | 抖动、栈水位、CPU 占用数据 |
| 每外设 ≥2 种传输方式 | UART 三种、CAN 中断+轮询、I2C 阻塞+DMA | 模式矩阵、对比测试 |
| 统一错误码与日志 | `Error`/`Result<T>` + 异步日志 | 故障注入表、日志截图 |
| 移植到另一块 STM32 | F103 主、F407 移植，UART+TIM 跑通 | 第二块板视频、移植 diff |

**最终交付**：Git 仓库（完整 commit）、`docs/` 六篇文档、演示视频、测试原始数据。

---

## 1. 当前起点

已经完成：

- Git 2.56、VS Code、ARM GNU Toolchain 14.3、CMake、Ninja、Ozone 3.3.2、J-Link 驱动。
- STM32CubeMX 6.18.1。
- F1 固件包（`D:\STM32Cube\Repository\STM32Cube_FW_F1_V1.8.7`）。
- F4 固件包（`D:\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.0`）。
- GitHub 仓库已联通，SSH 免密，`tools/git-save.ps1` 一键提交。
- 现有 commit：`04225b4` 初始化、`f600675` 脚本。

硬件：

- F103C8T6 核心板（主开发板）。
- CH340、MPU6050、TJA1050、SG90、编码器、按键、LED、J-Link v9。
- F407VET6 板已下单/在途（移植用）。

---

## 2. 总体路线

| 阶段 | 内容 | 预计 | 里程碑 |
|---|---|---|---|
| P1 | 点灯：时钟 + GPIO + 烧录 | 2 天 | `v0.1-blink` |
| P2 | 串口：USART1 + printf | 1 天 | `v0.1-uart` |
| P3 | 分层骨架：Error/Result/IUart | 2 天 | — |
| P4 | 日志系统 | 1 天 | — |
| P5 | UART 阻塞/中断/DMA | 3 天 | `v0.2-uart` |
| P6 | DWT + TIM2 + FreeRTOS | 3 天 | `v0.3-rtos` |
| P7 | CAN：收发 + bus-off | 2 天 | — |
| P8 | I2C + MPU6050 | 2 天 | — |
| P9 | 物资模块封装 | 3 天 | `v0.4-modules` |
| P10 | 系统任务 + 故障注入 | 2 天 | — |
| P11 | F407 移植 | 2 天 | `v0.5-port` |
| P12 | 文档 + 视频 + 发布 | 2 天 | `v1.0` |

合计约 **25 个工作日**。遇到板子/器件问题按 1.5 倍预留。

---

## 3. 目录规划

```text
brokytask01_framework/
├── app/                    # 业务层：main、robot_cmd、后续 gimbal/chassis
├── modules/                # 设备与算法：imu、motor、rc、servo、pid、filter
├── bsp/
│   ├── interface/          # 纯抽象：error.h、result.h、span.h、iuart.h、ican.h、ii2c.h
│   └── stm32/              # HAL 适配：hal_uart.cpp、hal_can.cpp、hal_i2c.cpp
├── board/
│   ├── f103/               # 主开发板：启动、链接、时钟、引脚
│   └── f407/               # 移植验证板
├── third_party/
│   ├── stm32f1/            # F1 HAL + CMSIS
│   ├── stm32f4/            # F4 HAL + CMSIS
│   └── freertos/           # FreeRTOS-Kernel
├── cmake/
│   └── toolchain-arm-none-eabi.cmake
├── tests/                  # 主机侧单元测试（协议、PID、滤波）
├── tools/                  # git-save.ps1、flash.jlink、串口脚本
├── docs/                   # 架构、接口、移植、测试报告
└── CMakeLists.txt
```

**依赖规则**：

1. 箭头只能向下：`app → modules → bsp/interface → bsp/stm32 → HAL`。
2. `app/`、`modules/` 禁止 `#include "stm32f1xx_hal.h"`，由 `tools/check_layering.ps1` 检查。
3. `board/` 是唯一知道“哪个 USART 接哪个引脚”的地方。
4. `bsp/interface` 不包含任何芯片头文件。

---

## 4. 阶段 1：点灯（详细）

目标：**F103 上 LED 以 1 Hz 闪烁，Ozone 能识别芯片**。这是整条工具链的第一次真实闭环。

### 4.1 拷入 F1 HAL 与 CMSIS

```powershell
cd C:\Work\brokytask01_framework

New-Item -ItemType Directory -Force `
  third_party\stm32f1\Drivers, `
  board\f103\inc, `
  bsp\interface, bsp\stm32, `
  modules, app, cmake, tools, docs | Out-Null

$fw = 'D:\STM32Cube\Repository\STM32Cube_FW_F1_V1.8.7'

Copy-Item -Recurse "$fw\Drivers\STM32F1xx_HAL_Driver" third_party\stm32f1\Drivers\
Copy-Item -Recurse "$fw\Drivers\CMSIS\Include"        third_party\stm32f1\Drivers\CMSIS\
Copy-Item -Recurse "$fw\Drivers\CMSIS\Device\ST\STM32F1xx" third_party\stm32f1\Drivers\CMSIS\Device\ST\
```

> 先整包拷，能跑通后再裁剪不用的 HAL 模块。F103 只有 64 KB Flash，后期要裁。

从包里拷板级文件：

```powershell
Copy-Item "$fw\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\gcc\startup_stm32f103xb.s" board\f103\
Copy-Item "$fw\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\gcc\linker\STM32F103XB_FLASH.ld" board\f103\linker.ld
Copy-Item "$fw\Drivers\CMSIS\Device\ST\STM32F1xx\Source\Templates\system_stm32f1xx.c" board\f103\
Copy-Item "$fw\Drivers\STM32F1xx_HAL_Driver\Inc\stm32f1xx_hal_conf_template.h" board\f103\inc\stm32f1xx_hal_conf.h
```

`stm32f1xx_hal_conf.h` 里确认这几个模块是打开的：

```c
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
```

并确认 `HSE_VALUE` 是 **8000000**。

### 4.2 `board/f103/inc/board.h`

```cpp
#pragma once
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 按你的板子原理图改：Blue Pill 一般是 PC13，低电平点亮 */
#define LED_Pin        GPIO_PIN_13
#define LED_GPIO_Port  GPIOC

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef  htim2;
extern I2C_HandleTypeDef  hi2c1;
extern CAN_HandleTypeDef  hcan1;

void board_init(void);
void board_led_write(uint8_t on);
void board_led_toggle(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
```

### 4.3 `board/f103/board.cpp`

```cpp
#include "board.h"

UART_HandleTypeDef huart1;
TIM_HandleTypeDef  htim2;
I2C_HandleTypeDef  hi2c1;
CAN_HandleTypeDef  hcan1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);

extern "C" void board_init(void) {
    HAL_Init();                 // 先用 SysTick 做 HAL 时基，接 FreeRTOS 后改 TIM4
    SystemClock_Config();
    MX_GPIO_Init();
}

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState        = RCC_HSE_ON;
    osc.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState    = RCC_PLL_ON;
    osc.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL      = RCC_PLL_MUL9;      // 8 MHz * 9 = 72 MHz
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;    // 72 MHz
    clk.APB1CLKDivider = RCC_HCLK_DIV2;      // 36 MHz
    clk.APB2CLKDivider = RCC_HCLK_DIV1;      // 72 MHz
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); // 默认灭
}

extern "C" void board_led_write(uint8_t on) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

extern "C" void board_led_toggle(void) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

extern "C" void Error_Handler(void) {
    __disable_irq();
    for (;;) {}
}
```

### 4.4 `board/f103/stm32f1xx_it.cpp`

```cpp
#include "board.h"

extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
}

extern "C" void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }
extern "C" void TIM2_IRQHandler(void)   { HAL_TIM_IRQHandler(&htim2); }
extern "C" void I2C1_EV_IRQHandler(void) { HAL_I2C_EV_IRQHandler(&hi2c1); }
extern "C" void I2C1_ER_IRQHandler(void) { HAL_I2C_ER_IRQHandler(&hi2c1); }
extern "C" void USB_LP_CAN1_RX0_IRQHandler(void) { HAL_CAN_IRQHandler(&hcan1); }
```

> 点灯阶段只用到 `SysTick_Handler`。后面接 FreeRTOS 时，`SysTick_Handler` 要交给 FreeRTOS（`#define xPortSysTickHandler SysTick_Handler`），HAL 时基改到 TIM4，这里要相应改。

### 4.5 `app/main.cpp`

```cpp
#include "board.h"

int main(void) {
    board_init();

    for (;;) {
        board_led_toggle();
        HAL_Delay(500);
    }
}
```

### 4.6 `cmake/toolchain-arm-none-eabi.cmake`

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_AR      arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE    arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

### 4.7 根 `CMakeLists.txt`（F103 版）

```cmake
cmake_minimum_required(VERSION 3.20)
project(brokytask01_framework LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT BOARD)
  set(BOARD "f103" CACHE STRING "f103 or f407")
endif()

if(BOARD STREQUAL "f103")
  set(CPU_FLAGS -mcpu=cortex-m3 -mthumb)
  set(BOARD_DIR ${CMAKE_SOURCE_DIR}/board/f103)
  set(HAL_TARGET stm32f1_hal)
elseif(BOARD STREQUAL "f407")
  set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
  set(BOARD_DIR ${CMAKE_SOURCE_DIR}/board/f407)
  set(HAL_TARGET stm32f4_hal)
else()
  message(FATAL_ERROR "Unknown BOARD=${BOARD}")
endif()

add_compile_options(${CPU_FLAGS} -Os -ffunction-sections -fdata-sections -Wall -Wextra)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)

add_link_options(
  ${CPU_FLAGS}
  -T${BOARD_DIR}/linker.ld
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map
  -specs=nano.specs
  -specs=nosys.specs
)

add_subdirectory(third_party)
add_subdirectory(bsp)
add_subdirectory(modules)
add_subdirectory(app)

add_executable(firmware
  ${BOARD_DIR}/startup_stm32f103xb.s
  ${BOARD_DIR}/system_stm32f1xx.c
  ${BOARD_DIR}/board.cpp
  ${BOARD_DIR}/stm32f1xx_it.cpp
)
target_include_directories(firmware PRIVATE ${BOARD_DIR}/inc)
target_link_libraries(firmware PRIVATE ${HAL_TARGET} bsp modules app)

add_custom_command(TARGET firmware POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:firmware> ${CMAKE_BINARY_DIR}/firmware.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware> ${CMAKE_BINARY_DIR}/firmware.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>
)
```

> 阶段 1 可以先不建 `bsp/modules/app` 子目录，直接把 `main.cpp` 放进 `firmware` 源列表；等阶段 3 再拆 target。

### 4.8 `third_party/CMakeLists.txt`

```cmake
add_subdirectory(stm32f1)
# 后面加：add_subdirectory(freertos)
```

`third_party/stm32f1/CMakeLists.txt` 列出 F1 HAL 源：

```cmake
set(HAL_SRC
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_can.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
)

add_library(stm32f1_hal STATIC ${HAL_SRC})
target_include_directories(stm32f1_hal PUBLIC
  Drivers/STM32F1xx_HAL_Driver/Inc
  Drivers/CMSIS/Device/ST/STM32F1xx/Include
  Drivers/CMSIS/Include
  ${CMAKE_SOURCE_DIR}/board/f103/inc
)
target_compile_definitions(stm32f1_hal PUBLIC
  USE_HAL_DRIVER
  STM32F103xB
)
```

### 4.9 编译

```powershell
cd C:\Work\brokytask01_framework
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake `
  -DBOARD=f103
cmake --build build
```

期望输出：

```text
   text    data     bss     dec     hex filename
   xxxxx     xxx    xxxx   xxxxx   xxxxx build/firmware.elf
```

### 4.10 烧录（Ozone）

1. J-Link 接 SWD：`3.3V / GND / SWDIO→PA13 / SWCLK→PA14`。
2. 打开 Ozone → **File → Open** → 选 `build/firmware.elf`。
3. 芯片选 **STM32F103C8**，Interface **SWD**，Speed 4000 kHz。
4. 点 **Download & Reset**。

也可以用 J-Link Commander 命令行：

```powershell
& "D:\J link驱动\JLink_V794k\JLink.exe" `
  -device STM32F103C8 -if SWD -speed 4000 -autoconnect 1 `
  -CommanderScript tools\flash.jlink
```

`tools/flash.jlink`：

```text
erase
loadfile build\firmware.hex
r
g
exit
```

### 4.11 验收 & 提交

- [ ] Ozone 能读到芯片 ID（`STM32F103C8`）
- [ ] LED 1 Hz 闪烁（亮 500 ms / 灭 500 ms）
- [ ] `arm-none-eabi-size` 显示 RAM/Flash 占用
- [ ] 断电重启后仍能运行（从 Flash 启动）

提交：

```powershell
.\tools\git-save.ps1 "feat(board): f103 clock and led bringup"
git tag -a v0.1-blink -m "F103 LED bringup"
git push origin v0.1-blink
```

---

## 5. 阶段 2：串口（1 天）

### 5.1 配置 USART1

- 引脚：PA9（TX，AF_PP）、PA10（RX，输入浮空/上拉）。
- 参数：115200、8N1、无流控。
- 时钟：USART1 在 APB2（72 MHz）。

`board.cpp` 增加：

```cpp
static void MX_USART1_UART_Init(void) {
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}
```

MSP 里配 PA9/PA10：

```cpp
__HAL_RCC_USART1_CLK_ENABLE();
__HAL_RCC_GPIOA_CLK_ENABLE();

GPIO_InitTypeDef gpio = {0};
gpio.Pin   = GPIO_PIN_9;
gpio.Mode  = GPIO_MODE_AF_PP;
gpio.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(GPIOA, &gpio);

gpio.Pin  = GPIO_PIN_10;
gpio.Mode = GPIO_MODE_INPUT;
gpio.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOA, &gpio);
```

### 5.2 printf 重定向

```cpp
extern "C" int _write(int, char* ptr, int len) {
    HAL_UART_Transmit(&huart1, reinterpret_cast<uint8_t*>(ptr),
                      static_cast<uint16_t>(len), 100);
    return len;
}
```

`main.cpp`：

```cpp
std::printf("hello from f103, sysclk=%lu\r\n",
            static_cast<unsigned long>(HAL_RCC_GetHCLKFreq()));
```

### 5.3 验收

- [ ] 串口 115200 输出 `hello from f103, sysclk=72000000`
- [ ] 每秒打印一次 `tick`
- [ ] 拔掉 TX 不卡死（`HAL_UART_Transmit` 有超时）
- [ ] 提交 `feat(board): uart1 hello`

> 参考 basic_framework：它的 `bsp_usart` 也是“实例 + 注册回调 + 独立测试”的思路；我们这一步先用阻塞发送把链路打通，阶段 5 再补中断/DMA。

---

## 6. 阶段 3：分层骨架（2 天）

### 6.1 接口层

新增：

```
bsp/interface/error.h      Error 枚举 + to_string
bsp/interface/result.h     Result<T>
bsp/interface/span.h       ByteSpan（C++17 无 std::span）
bsp/interface/iuart.h      IUart + UartConfig
```

`IUart` 设计：

```cpp
class IUart {
public:
    using RxCallback = void (*)(void* ctx, ByteSpan data);

    virtual ~IUart() = default;
    virtual Error init(const UartConfig& cfg) = 0;
    virtual Error write(ByteSpan data, uint32_t timeout_ms) = 0;
    virtual Error writeAsync(ByteSpan data) = 0;
    virtual Error read(ByteSpan out, uint32_t timeout_ms) = 0;
    virtual Error startRx() = 0;
    virtual void setRxCallback(RxCallback cb, void* ctx) = 0;
};
```

### 6.2 适配层

`bsp/stm32/hal_uart.cpp` 实现 `HalUart : IUart`，只在这里包含 HAL。

### 6.3 CMake target 拆分

```cmake
add_library(bsp OBJECT interface/error.cpp stm32/hal_uart.cpp)
target_include_directories(bsp
  PUBLIC  interface
  PRIVATE stm32
)
target_link_libraries(bsp PRIVATE stm32f1_hal)

add_library(modules OBJECT modules.cpp)
target_link_libraries(modules PUBLIC bsp)

add_library(app OBJECT main.cpp)
target_link_libraries(app PUBLIC modules)
```

关键：`bsp` 对 `stm32f1_hal` 用 **PRIVATE**，HAL 头文件不会传播到 `modules/app`。

### 6.4 分层检查脚本

`tools/check_layering.ps1`：

```powershell
$bad = Get-ChildItem app, modules -Recurse -Include *.c,*.cpp,*.h,*.hpp |
       Select-String -Pattern 'stm32f1xx_hal\.h|stm32f4xx_hal\.h'
if ($bad) { $bad; exit 1 }
Write-Host "layering OK"
```

### 6.5 对照 basic_framework

| basic_framework | 本方案 |
|---|---|
| `XXXInstance` + `XXXRegister(config)` | C++ 类 + `init(config)` |
| 回调 + `void*` 父指针 | `RxCallback` + `void* ctx` |
| `bsp/module/app` 三层 | 同样三层，多拆 `interface` 与 `stm32` |
| HAL 强耦合 | HAL 只在 `bsp/stm32` |
| C 风格 | C++17，禁异常/RTTI |

### 6.6 验收

- [ ] `app/modules` 里 grep 不到 HAL 头文件
- [ ] `check_layering.ps1` 通过
- [ ] `bsp`、`modules`、`app` 三个 target 能单独编译
- [ ] 提交 `refactor: split bsp interface and stm32 adapter`

---

## 7. 阶段 4：错误码与日志（1 天）

### 7.1 错误码

```cpp
enum class Error : uint16_t {
    Ok = 0, NotInitialized, InvalidArg, Busy, Timeout,
    Hardware, Checksum, Overflow, NotSupported, Offline, Unknown
};

template <typename T> class Result { /* ... */ };
```

规则：

- 所有可能失败的接口返回 `Error` 或 `Result<T>`，加 `[[nodiscard]]`。
- HAL 状态映射：`HAL_OK→Ok`、`HAL_TIMEOUT→Timeout`、`HAL_BUSY→Busy`、`HAL_ERROR→Hardware`。
- 禁止 `while(1)` 死等。

### 7.2 日志

```cpp
LOG_INFO("imu", "bias=(%.3f, %.3f, %.3f)", bx, by, bz);
LOG_ERROR("can", "tx timeout, id=0x%03X", id);
```

- 级别：Trace/Debug/Info/Warn/Error/Fatal。
- 编译期 `LOG_LEVEL` 裁剪。
- DWT 时间戳、模块 tag。
- 生产者格式化到栈缓冲 → 队列 → logger 任务 → UART DMA。
- ISR 禁止格式化；同一条日志限流。

### 7.3 失败策略表

| 场景 | 返回 | 动作 |
|---|---|---|
| 初始化失败 | `Hardware` / `NotInitialized` | 安全态 + LED/蜂鸣器 |
| 通信超时 | `Timeout` | 重试 3 次 → 离线 |
| CRC 错 | `Checksum` | 丢帧计数，连续 N 次离线 |
| 参数越界 | `InvalidArg` | 拒绝并回 NAK |
| 忙 | `Busy` | 调用方退避 |

### 7.4 验收

- [ ] 人为拔掉 I2C/CAN，返回错误码并打日志，不死机
- [ ] 日志多任务不串行
- [ ] 提交 `feat(core): error code and async logger`

---

## 8. 阶段 5：UART 三种模式（3 天）

### 8.1 阻塞

阶段 2 已完成；补超时测试。

### 8.2 中断接收

- 单字节中断 + 环形缓冲（SPSC）。
- ISR 只搬运 + `vTaskNotifyGiveFromISR`（接 RTOS 后）。
- 缓冲满策略：丢新/丢旧/计数，三选一写文档。

### 8.3 DMA 发送/接收

- 发送：`HAL_UART_Transmit_DMA` + 完成回调 + `Busy` 状态。
- 接收：`HAL_UART_ReceiveToIdle_DMA` 或 DMA circular + IDLE。
- 双缓冲，避免半传输覆盖。

### 8.4 测试

- 回环：PA9–PA10 短接，收发一致。
- 不定长帧：PC 每 1 ms 发一帧，10 分钟不丢。
- 1 kHz × 100 字节遥测，10 分钟不丢。
- 对比 CPU 占用：阻塞 vs 中断 vs DMA。

### 8.5 文档中的模式矩阵

| 模式 | CPU | 吞吐 | 复杂度 | 适用 |
|---|---|---|---|---|
| 阻塞 | 高 | 低 | 低 | 初始化、短命令 |
| 中断 | 中 | 中 | 中 | 命令帧、低频传感器 |
| DMA | 低 | 高 | 高 | 1 kHz 遥测、日志 |

### 8.6 验收

- [ ] 三种模式都有独立测试
- [ ] 模式矩阵写进 `docs/03-interface.md`
- [ ] tag `v0.2-uart`

---

## 9. 阶段 6：DWT + TIM2 + FreeRTOS（3 天）

### 9.1 DWT

```cpp
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

提供 `dwt_us()`、`dwt_delta_us()`。

### 9.2 TIM2 1 kHz

- PSC = 71，ARR = 999（72 MHz / 72 / 1000）。
- 中断里只 `vTaskNotifyGiveFromISR`，不读外设、不算浮点。

### 9.3 FreeRTOS

- 源码：`FreeRTOS-Kernel` 的 `include`、`portable/GCC/ARM_CM3`、`heap_4.c`。
- `configCPU_CLOCK_HZ = 72000000`、`configTICK_RATE_HZ = 1000`。
- HAL 时基改 **TIM4**，SysTick 交给 FreeRTOS：
  ```c
  #define xPortSysTickHandler SysTick_Handler
  #define vPortSVCHandler     SVC_Handler
  #define xPortPendSVHandler  PendSV_Handler
  ```
- 删除自己写的 `SysTick_Handler`，否则重复定义。

### 9.4 任务表

| 任务 | 优先级 | 周期 | 栈（字） | 通信 |
|---|---|---|---|---|
| `task_imu` | 6 | 1 ms | 128 | 定时器/EXTI 通知 |
| `task_control` | 5 | 1 ms | 384 | 队列收指令 |
| `task_comm` | 4 | 事件 | 384 | 流缓冲/队列 |
| `task_safety` | 4 | 10 ms | 128 | 事件组 |
| `task_telemetry` | 2 | 10 ms | 256 | 队列 |
| `task_logger` | 1 | 事件 | 128 | 环形缓冲 |

F103 只有 20 KB RAM，总栈约 5.6 KB，加 idle/timer、缓冲、HAL，控制在 10 KB 以内；用 `uxTaskGetStackHighWaterMark` 逐个调。

### 9.5 验收

- [ ] 6 个任务稳定运行 1 小时
- [ ] 1 kHz 抖动 < ±50 µs（DWT 统计 max/avg/σ）
- [ ] 栈水位、CPU 占用有数据
- [ ] 提交 `feat(rtos): task framework and 1kHz tick`，tag `v0.3-rtos`

---

## 10. 阶段 7：CAN（2 天）

- 收发器：TJA1050 / SN65HVD230，3.3V 供电优先 230。
- 引脚：PB8/PB9，需要 `__HAL_RCC_AFIO_CLK_ENABLE()` + `__HAL_AFIO_REMAP_CAN1_2()`。
- 波特率 500 kbps：APB1 36 MHz，PSC=4，BS1=11，BS2=2 → 36/4/18 = 500 k。
- 过滤器：32 位掩码模式，先全通过，再按 ID 收窄。
- 中断接收 → 队列；发送邮箱轮询/中断。
- bus-off：`HAL_CAN_ErrorCallback` 里 `Stop` + `Start`，记录错误计数。
- 测试：双板互发 1 万帧；USB-CAN 抓包；拔线恢复。

验收：

- [ ] 1 万帧不丢
- [ ] bus-off 能恢复
- [ ] 终端电阻两端各 120 Ω
- [ ] 提交 `feat(bsp): can interrupt rx and bus-off recovery`

---

## 11. 阶段 8：I2C + MPU6050（2 天）

- I2C1：PB6/PB7，400 kHz，7 位地址。
- `WHO_AM_I`（0x75）应读回 `0x68`。
- 初始化：PWR_MGMT_1=0x01、SMPLRT_DIV=0、CONFIG=0x03、GYRO_CONFIG=0x08、ACCEL_CONFIG=0x08。
- 读 14 字节（0x3B 起）：加速度 6 + 温度 2 + 陀螺 6。
- INT 引脚 → EXTI → 任务通知。
- 上电静止 2 s 校准零偏；运行期静止检测 + 慢零偏跟踪。
- 姿态：先互补滤波，再 Mahony；F103 无 FPU，任务 01 阶段 100–200 Hz 即可。

验收：

- [ ] 静态 10 分钟漂移 < 1°
- [ ] 转动 90° 跟踪无明显滞后
- [ ] 校准失败返回错误码
- [ ] 提交 `feat(module): mpu6050 driver and attitude`

---

## 12. 阶段 9：物资模块封装（3 天）

按清单逐项封装，统一 `IModule`：

```cpp
class IModule {
public:
    virtual ~IModule() = default;
    virtual Error init() = 0;
    virtual Error update() = 0;
    virtual bool online() const = 0;
};
```

| 物资 | 模块 | 接口 |
|---|---|---|
| CH340 | 已作为 USART1 日志口 | `IUart` |
| MPU6050 | `modules/imu` | `read()` / `attitude()` |
| TJA1050 | `bsp/stm32/hal_can` | `ICan` |
| SG90 | `modules/servo` | `setAngle()`（PWM，TIM1 CH1 PA8） |
| 激光管 | `modules/gpio` | `on/off` |
| 编码器 | `modules/encoder` | `getCount()`（TIM3 编码器模式） |
| 按键 | `modules/button` | EXTI + 消抖 |
| LED | `board` | `board_led_write()` |

每个模块一个独立测试入口（参考 basic_framework 的“单 bsp/module 测试”做法）：

```
tests/test_imu.cpp
tests/test_can.cpp
tests/test_servo.cpp
```

验收：

- [ ] 每个模块脱离业务能单独跑
- [ ] 离线检测有返回值
- [ ] tag `v0.4-modules`

---

## 13. 阶段 10：系统任务 + 故障注入（2 天）

- 控制任务 1 kHz：TIM2 通知 → 读 IMU → 计算 → 输出。
- 通信任务事件驱动；安全任务 10 ms；遥测 100 Hz；日志最低。
- 看门狗 IWDG（1 s），各任务心跳位都置位才喂狗。
- 栈溢出 hook、malloc 失败 hook。
- 故障注入表：

| 注入 | 期望 |
|---|---|
| 拔 CAN 线 | `Offline`，降额/停机 |
| I2C 不应答 | `Timeout`，重试 3 次后 IMU 故障 |
| 串口 CRC 错 | `Checksum`，丢帧计数 |
| 参数越界 | `InvalidArg`，回 NAK |
| 栈溢出 | hook 记录任务名，安全停机 |

验收：

- [ ] 无静默失败
- [ ] 拔线/超时/越界都有日志和返回值
- [ ] 提交 `feat(system): task supervision and fault injection`

---

## 14. 阶段 11：F407 移植（2 天）

- `board/f407`：168 MHz、USART1、TIM6 1 kHz、I2C1、CAN1。
- CMake：`-DBOARD=f407`，`-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard`。
- HAL 用 F4 包，启动 `startup_stm32f407xx.s`，链接 `STM32F407VETX_FLASH.ld`（VET6：512 KB / 128+64 KB）。
- FreeRTOS 换 `ARM_CM4F`。
- **app/modules 零修改**，只改 board/CMake。
- 验收：串口打印 + TIM6 1 kHz 翻转 LED + 日志。
- 记录移植 diff 和资源对比表。
- 提交 `feat(board): f407 port with uart and timer`，tag `v0.5-port`。

---

## 15. 阶段 12：文档 + 视频 + 发布（2 天）

### 15.1 文档

```
docs/
├── 01-architecture.md     架构图、分层规则、任务表
├── 02-directory.md        目录职责
├── 03-interface.md        接口表：签名/参数/返回/线程安全
├── 04-porting.md          F103→F407 步骤、diff、踩坑
├── 05-test-report.md      测试方法、原始数据、结论
└── 06-hardware.md         BOM、引脚表、接线图
```

### 15.2 视频分镜（5 分钟）

| 时间 | 内容 |
|---|---|
| 0:00–0:20 | 硬件全景、任务目标 |
| 0:20–0:50 | 架构图/目录 |
| 0:50–2:00 | UART 三模式、CAN、I2C 演示 |
| 2:00–2:50 | 1 kHz 抖动、CPU/栈数据 |
| 2:50–3:40 | 故障注入（拔线、超时） |
| 3:40–4:30 | F407 移植演示 |
| 4:30–5:00 | 总结与不足 |

### 15.3 发布

```powershell
.\tools\git-save.ps1 "docs: complete task01 documentation"
git tag -a v1.0 -m "Task 01 framework v1.0"
git push origin v1.0
```

---

## 16. 提交与 tag 清单

| 阶段 | commit 信息 | tag |
|---|---|---|
| 点灯 | `feat(board): f103 clock and led bringup` | `v0.1-blink` |
| 串口 | `feat(board): uart1 hello` | — |
| 分层 | `refactor: split bsp interface and stm32 adapter` | — |
| 错误/日志 | `feat(core): error code and async logger` | — |
| UART 三模式 | `feat(bsp): uart it and dma modes` | `v0.2-uart` |
| RTOS | `feat(rtos): task framework and 1kHz tick` | `v0.3-rtos` |
| CAN | `feat(bsp): can interrupt rx and bus-off recovery` | — |
| I2C/MPU | `feat(module): mpu6050 driver and attitude` | — |
| 模块 | `feat(modules): servo encoder button laser` | `v0.4-modules` |
| 系统 | `feat(system): task supervision and fault injection` | — |
| 移植 | `feat(board): f407 port with uart and timer` | `v0.5-port` |
| 文档 | `docs: complete task01 documentation` | `v1.0` |

---

## 17. 风险与应对

| 风险 | 影响 | 应对 |
|---|---|---|
| F407 板延迟到货 | 移植阶段卡住 | 先做 F103 全部功能；F407 到了再补 |
| F103 只有 20 KB RAM | 任务多、缓冲大时不够 | 静态分配、减小栈、日志限流 |
| 无 FPU | 姿态解算慢 | 100–200 Hz、定点/查表、I2C DMA |
| 64 KB Flash | HAL + RTOS + app 紧张 | `-Os`、裁剪 HAL、避免 iostream |
| J-Link 接线/驱动 | 烧录失败 | 先读 ID；检查 GND/SWDIO/SWCLK；Ozone 选对芯片 |
| CAN 终端电阻 | 通信不稳 | 两端各 120 Ω，先 125 kbps 测试 |
| I2C 总线锁死 | 传感器离线 | 9 clock 恢复、上拉、降速 |
| FreeRTOS 优先级 | 偶发死机 | ISR 优先级 ≥ 5、`FromISR` API、栈溢出 hook |
| 范围蔓延 | 25 天做不完 | 先完成五条硬要求，再加 SPI/Flash/Shell |

---

## 18. 题目要求对照

| 题目要求 | 对应阶段 | 证据 |
|---|---|---|
| UART/USART、CAN、I2C 封装 | P2、P5、P7、P8 | 接口文件、测试、波形 |
| 物资封装 | P9 | 单模块测试程序 |
| 模块化、高扩展 | P3 | 目录、CMake target、分层检查 |
| C++17 分层 | P3 | 架构图、代码 |
| 业务不调 HAL | P3、P11 | grep 结果、F407 零修改 |
| FreeRTOS 任务划分 | P6 | 任务表、抖动/栈/CPU |
| 每种外设 ≥2 模式 | P5、P7、P8 | 模式矩阵、对比数据 |
| 统一错误码/日志 | P4、P10 | 错误码表、故障注入表 |
| 移植另一块 STM32 | P11 | 第二块板视频、diff |
| Git 完整记录 | 全程 | commit/tag 历史 |

---

## 19. 参考 basic_framework 的阅读顺序

1. `README.md` 的“架构 / 设计思想 / 执行顺序”三节。
2. `.Doc/架构介绍与开发指南.md`：文件树、命名规范。
3. `bsp/usart/`：`XXXInstance` + `XXXRegister(config)` + 回调。
4. `module/` 里任意电机：如何包含 bsp、暴露硬件无关接口。
5. `app/robot_task`：任务如何创建、优先级如何安排。
6. 每个 bsp/module 目录下的独立测试文档：我们照这个思路做 `tests/`。

**只学结构和接口设计，不抄实现**：它的 C 风格 `this` 指针、HAL 强耦合，正是本方案要替换的部分。
