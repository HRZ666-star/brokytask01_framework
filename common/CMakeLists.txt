add_library(common STATIC
    error/error.cpp
)

target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_features(common PUBLIC cxx_std_17)
