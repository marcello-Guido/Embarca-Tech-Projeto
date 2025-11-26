add_library(AHT10
    aht10.c
)

target_include_directories(AHT10 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(AHT10
    hardware_i2c
    pico_stdlib
)