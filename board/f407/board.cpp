#include "board.h"
#include "board_uart.h"
#include "bsp/stm32/uart/hal_uart.h"

UART_HandleTypeDef huart1;
TIM_HandleTypeDef  htim2;

static bsp::HalUart g_uart{huart1};
static volatile uint32_t g_timer_ticks = 0;

extern "C" {
volatile uint32_t g_fatal_error_code = 0U;
volatile const char* g_fatal_file = nullptr;
volatile uint32_t g_fatal_line = 0U;
}

static common::Error SystemClock_Config(void);
static common::Error MX_GPIO_Init(void);
static common::Error MX_USART1_UART_Init(void);
static common::Error MX_TIM2_Init(void);

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

    error = hal_to_error(HAL_InitTick(TICK_INT_PRIORITY));
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

    return g_uart.init({115200U, bsp::UartMode::Blocking});
}

bsp::IUart& board_uart() { return g_uart; }

uint32_t board_timer_ticks(void) { return g_timer_ticks; }

static common::Error SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {};
    RCC_ClkInitTypeDef clk = {};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8U;      // 8 MHz / 8 = 1 MHz
    osc.PLL.PLLN       = 336U;    // 1 MHz * 336 = 336 MHz
    osc.PLL.PLLP       = RCC_PLLP_DIV2; // 168 MHz
    osc.PLL.PLLQ       = 7U;      // 48 MHz USB/SDIO
    common::Error error = hal_to_error(HAL_RCC_OscConfig(&osc));
    if (error != common::Error::Ok) {
        return error;
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;   // 42 MHz
    clk.APB2CLKDivider = RCC_HCLK_DIV2;   // 84 MHz
    return hal_to_error(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5));
}

static common::Error MX_GPIO_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); // active low, off
    return common::Error::Ok;
}

static common::Error MX_USART1_UART_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10; // TX, RX
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance = USART1;

    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    return common::Error::Ok;
}

static common::Error MX_TIM2_Init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 84U - 1U;  // 84 MHz / 84 = 1 MHz
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 1000U - 1U; // 1 kHz
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    common::Error error = hal_to_error(HAL_TIM_Base_Init(&htim2));
    if (error != common::Error::Ok) {
        return error;
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    return hal_to_error(HAL_TIM_Base_Start_IT(&htim2));
}

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (htim == &htim2) {
        ++g_timer_ticks;
    }
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
