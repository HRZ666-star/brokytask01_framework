# Header-only interface layer.  It must never include a HAL header.
add_library(bsp_interface INTERFACE)

target_include_directories(bsp_interface INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(bsp_interface INTERFACE common)
target_compile_features(bsp_interface INTERFACE cxx_std_17)
