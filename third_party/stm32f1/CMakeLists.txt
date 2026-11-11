set(HAL_SRC
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
  Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c
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
