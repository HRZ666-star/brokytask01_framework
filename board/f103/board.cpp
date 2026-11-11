#include "board.h"

UART_HandleTypeDef huart1;
TIM_HandleTypeDef  htim2;
I2C_HandleTypeDef  hi2c1;
CAN_HandleTypeDef  hcan1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);

extern "C" void board_init(void) {
    HAL_Init();                 // SysTick timebase for now; FreeRTOS will move HAL tick to TIM4
    SystemClock_Config();
    MX_GPIO_Init();
}

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {};
    RCC_ClkInitTypeDef clk = {};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;   // 8 MHz * 9 = 72 MHz
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef gpio = {};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); // off
}

extern "C" void board_led_write(uint8_t on) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

extern "C" void board_led_toggle(void) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

extern "C" void Error_Handler(void) {
    __disable_irq();
    for (;;) {
    }
}
