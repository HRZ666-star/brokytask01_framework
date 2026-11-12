set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(FREERTOS_SRC
  ${FREERTOS_DIR}/croutine.c
  ${FREERTOS_DIR}/event_groups.c
  ${FREERTOS_DIR}/list.c
  ${FREERTOS_DIR}/queue.c
  ${FREERTOS_DIR}/stream_buffer.c
  ${FREERTOS_DIR}/tasks.c
  ${FREERTOS_DIR}/timers.c
  ${FREERTOS_DIR}/portable/GCC/ARM_CM3/port.c
  ${FREERTOS_DIR}/portable/heap_4.c
)

add_library(freertos STATIC ${FREERTOS_SRC})

target_include_directories(freertos PUBLIC
  ${FREERTOS_DIR}/include
  ${FREERTOS_DIR}/portable/GCC/ARM_CM3
  ${CMAKE_SOURCE_DIR}/board/f103/inc
)

target_compile_definitions(freertos PUBLIC
  USE_HAL_DRIVER
  STM32F103xB
)
