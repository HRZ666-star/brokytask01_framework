#include "board.h"
#include "board_can.h"
#include "board_i2c.h"
#include "board_pwm.h"
#include "board_uart.h"
#include "bsp/stm32/can/hal_can.h"
#include "bsp/stm32/i2c/hal_i2c.h"
#include "bsp/stm32/pwm/hal_pwm.h"
#include "bsp/stm32/uart/hal_uart.h"
#include "stm32f1xx_hal_gpio_ex.h"

UART_HandleTypeDef huart1;
TIM_HandleTypeDef  htim2;
TIM_HandleTypeDef  htim3;
I2C_HandleTypeDef  hi2c1;
CAN_HandleTypeDef  hcan1;

static bsp::HalUart g_uart{huart1};
static bsp::HalCan  g_can{hcan1};
static bsp::HalI2c  g_i2c{hi2c1};
static bsp::HalPwm  g_pwm{htim3, TIM_CHANNEL_1};

extern "C" {
volatile uint32_t g_fatal_error_code = 0U;
volatile const char* g_fatal_file = nullptr;
volatile uint32_t g_fatal_line = 0U;
}

static common::Error SystemClock_Config(void);
static common::Error MX_GPIO_Init(void);
static common::Error MX_USART1_UART_Init(void);
static common::Error MX_TIM2_Init(void);
static common::Error MX_TIM3_Init(void);
static common::Error MX_I2C1_Init(void);
static common::Error MX_CAN_Init(void);

static common::Error hal_to_error(HAL_StatusTypeDef status) {
    switch (status) {
        case HAL_OK:      return common::Error::Ok;
        case HAL_BUSY:    return common::Error::Busy;
        case HAL_TIMEOUT: return common::Error::Timeout;
        case HAL_ERROR:
        default:          return common::Error::Hardware;
    }
}

extern "C" common::Error board_init(void) {
    common::Error error = hal_to_error(HAL_Init());
    if (error != common::Error::Ok) {
        return error;
    }

    error = SystemClock_Config();
    if (error != common::Error::Ok) {
        return error;
    }

    error = MX_GPIO_Init();
    if (error != common::Error::Ok) {
        return error;
    }

    error = MX_USART1_UART_Init();
    if (error != common::Error::Ok) {
        return error;
    }

    error = MX_TIM2_Init();
    if (error != common::Error::Ok) {
        return error;
    }

    error = MX_TIM3_Init();
    if (error != common::Error::Ok) {
        return error;
    }

    error = MX_I2C1_Init();
    if (error != common::Error::Ok) {
        return error;
    }

    error = MX_CAN_Init();
    if (error != common::Error::Ok) {
        return error;
    }

    return g_uart.init({115200U, bsp::UartMode::Blocking});
}

bsp::IUart& board_uart() { return g_uart; }
bsp::II2c& board_i2c() { return g_i2c; }
bsp::ICan& board_can() { return g_can; }
bsp::IPwm& board_pwm() { return g_pwm; }

static common::Error SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {};
    RCC_ClkInitTypeDef clk = {};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;   // 8 MHz * 9 = 72 MHz
    common::Error error = hal_to_error(HAL_RCC_OscConfig(&osc));
    if (error != common::Error::Ok) {
        return error;
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    return hal_to_error(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2));
}

static common::Error MX_GPIO_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); // off
    return common::Error::Ok;
}

static common::Error MX_USART1_UART_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin   = GPIO_PIN_9;            // TX
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin  = GPIO_PIN_10;            // RX
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance = USART1;

    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    return common::Error::Ok;
}

/*
 * Free-running 1 MHz time base for period/jitter measurements.
 * APB1 is 36 MHz and the timer clock is doubled to 72 MHz, so /72 gives 1 us.
 */
static common::Error MX_TIM2_Init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    /* 16-bit wrap window (65.536 ms); the task subtracts 16-bit deltas. */
    htim2.Init.Period = 0xFFFFU;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    common::Error error = hal_to_error(HAL_TIM_Base_Init(&htim2));
    if (error != common::Error::Ok) {
        return error;
    }
    return hal_to_error(HAL_TIM_Base_Start(&htim2));
}

uint32_t board_micros(void) {
    return __HAL_TIM_GET_COUNTER(&htim2);
}

/*
 * GPIO/clock for the servo signal (PA6 / TIM3_CH1).  The PWM duration and
 * channel configuration belong to HalPwm::init(), which owns the 1 us tick.
 */
static common::Error MX_TIM3_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    htim3.Instance = TIM3;
    return common::Error::Ok;
}

static common::Error MX_I2C1_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;   // SCL=PB6, SDA=PB7
    gpio.Mode  = GPIO_MODE_AF_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    hi2c1.Instance = I2C1;

    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
    return common::Error::Ok;
}

static common::Error MX_CAN_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_AFIO_REMAP_CAN1_2();      // CAN_RX=PB8, CAN_TX=PB9

    gpio.Pin  = GPIO_PIN_8;         // CAN_RX
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin   = GPIO_PIN_9;        // CAN_TX
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    hcan1.Instance = CAN1;

    HAL_NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
    HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
    return common::Error::Ok;
}

extern "C" int _write(int, char* ptr, int len) {
    if (len <= 0) {
        return 0;
    }
    const common::Error error = g_uart.write(
        {reinterpret_cast<const std::uint8_t*>(ptr),
         static_cast<std::size_t>(len)},
        100U);
    return error == common::Error::Ok ? len : -1;
}

extern "C" void board_led_write(uint8_t on) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

extern "C" void board_led_toggle(void) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

[[noreturn]] void fatal_error(common::Error error, const char* file, int line) {
    g_fatal_error_code = static_cast<std::uint32_t>(error);
    g_fatal_file = file;
    g_fatal_line = static_cast<std::uint32_t>(line);
    __disable_irq();
    for (;;) {
        __NOP();
    }
}

extern "C" void Error_Handler(void) {
    fatal_error(common::Error::Unknown, __FILE__, __LINE__);
}
