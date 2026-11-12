cmake_minimum_required(VERSION 3.20)
project(brokytask01_framework LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT BOARD)
  set(BOARD "f103" CACHE STRING "Target board: f103 | f407")
endif()

# Application entry point.  The default is the normal demo; the smoke test is
# a separate image so a failed scheduler bring-up cannot disturb the UART demo.
set(APP "main" CACHE STRING "Application entry: main | freertos_smoke")
if(APP STREQUAL "freertos_smoke")
  set(APP_SOURCE ${CMAKE_SOURCE_DIR}/app/freertos_smoke.cpp)
  set(APP_DEFINITIONS USE_FREERTOS)
elseif(APP STREQUAL "main")
  set(APP_SOURCE ${CMAKE_SOURCE_DIR}/app/main.cpp)
  set(APP_DEFINITIONS)
else()
  message(FATAL_ERROR "Unknown APP=${APP} (expected main or freertos_smoke)")
endif()

if(BOARD STREQUAL "f103")
  set(CPU_FLAGS -mcpu=cortex-m3 -mthumb)
  set(BOARD_DIR ${CMAKE_SOURCE_DIR}/board/f103)
  set(HAL_TARGET stm32f1_hal)
elseif(BOARD STREQUAL "f407")
  message(FATAL_ERROR "F407 board layer is not added yet")
else()
  message(FATAL_ERROR "Unknown BOARD=${BOARD}")
endif()

add_compile_options(${CPU_FLAGS} -Os -g3 -ffunction-sections -fdata-sections -Wall -Wextra)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

add_link_options(
  ${CPU_FLAGS}
  -T${BOARD_DIR}/linker.ld
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map
  -specs=nano.specs
  -specs=nosys.specs
)

add_subdirectory(third_party)

add_executable(firmware
  ${BOARD_DIR}/startup_stm32f103xb.s
  ${BOARD_DIR}/system_stm32f1xx.c
  ${BOARD_DIR}/board.cpp
  ${BOARD_DIR}/stm32f1xx_it.cpp
  ${APP_SOURCE}
)

target_include_directories(firmware PRIVATE ${BOARD_DIR}/inc)
target_link_libraries(firmware PRIVATE ${HAL_TARGET} freertos)
if(APP_DEFINITIONS)
  target_compile_definitions(firmware PRIVATE ${APP_DEFINITIONS})
endif()
set_target_properties(firmware PROPERTIES SUFFIX ".elf")

add_custom_command(TARGET firmware POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:firmware> ${CMAKE_BINARY_DIR}/firmware.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware> ${CMAKE_BINARY_DIR}/firmware.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>
)
