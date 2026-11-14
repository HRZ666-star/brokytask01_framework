# Module layer: devices and reusable building blocks.
# Modules may use BSP interfaces and FreeRTOS, but must not include HAL headers.
add_library(modules STATIC
    logger/logger.cpp
)

target_include_directories(modules PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(modules PUBLIC common bsp_interface freertos)
target_compile_features(modules PUBLIC cxx_std_17)
