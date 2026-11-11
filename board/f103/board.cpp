#include "board.h"

UART_HandleTypeDef huart1;
TIM_HandleTypeDef  htim2;
I2C_HandleTypeDef  hi2c1;
CAN_HandleTypeDef  hcan1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

extern "C" void board_init(void) {
    HAL_Init();                 // SysTick timebase for now; FreeRTOS will move HAL tick to TIM4
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
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

static void MX_USART1_UART_Init(void) {
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

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

extern "C" int _write(int, char* ptr, int len) {
    HAL_UART_Transmit(&huart1, reinterpret_cast<uint8_t*>(ptr),
                      static_cast<uint16_t>(len), 100);
    return len;
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
