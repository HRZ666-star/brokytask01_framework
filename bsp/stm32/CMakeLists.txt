# STM32 adapter layer.  This is the only place allowed to include HAL headers.
set(BSP_STM32_SOURCES
    uart/hal_uart.cpp
)

# F407 bring-up only needs UART.  I2C/CAN adapters stay F103-only until the
# F407 board layer initialises those peripherals.
if(BOARD STREQUAL "f103")
  list(APPEND BSP_STM32_SOURCES
      can/hal_can.cpp
  )
endif()

add_library(bsp_stm32 STATIC ${BSP_STM32_SOURCES})

target_include_directories(bsp_stm32 PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(bsp_stm32 PUBLIC bsp_interface ${HAL_TARGET})
target_compile_features(bsp_stm32 PUBLIC cxx_std_17)
