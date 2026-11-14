# STM32 adapter layer.  This is the only place allowed to include HAL headers.
add_library(bsp_stm32 STATIC
    uart/hal_uart.cpp
    can/hal_can.cpp
)

target_include_directories(bsp_stm32 PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(bsp_stm32 PUBLIC bsp_interface ${HAL_TARGET})
target_compile_features(bsp_stm32 PUBLIC cxx_std_17)
