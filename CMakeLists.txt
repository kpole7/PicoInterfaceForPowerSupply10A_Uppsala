cmake_minimum_required(VERSION 3.13)
project(power_source_10A_Uppsala C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(power_source_10A_Uppsala
    ${CMAKE_CURRENT_LIST_DIR}/source/power_source.c
    ${CMAKE_CURRENT_LIST_DIR}/source/main_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/source/rstl_protocol.c
    ${CMAKE_CURRENT_LIST_DIR}/source/uart_talks.c
    ${CMAKE_CURRENT_LIST_DIR}/source/psu_talks.c
    ${CMAKE_CURRENT_LIST_DIR}/source/writing_to_dac.c
    ${CMAKE_CURRENT_LIST_DIR}/source/pwm_output.c
    ${CMAKE_CURRENT_LIST_DIR}/source/i2c_outputs.c
    ${CMAKE_CURRENT_LIST_DIR}/source/adc_inputs.c
    ${CMAKE_CURRENT_LIST_DIR}/source/compilation_time.c
    ${CMAKE_CURRENT_LIST_DIR}/source/debugging.c
)

add_custom_target(
    force_compilation_time ALL
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/phony_timestamp
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/phony_timestamp
    COMMENT "Touch phony_timestamp to force recompilation of compilation_time.c"
)

set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/source/compilation_time.c
    PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/phony_timestamp
)

target_compile_definitions(power_source_10A_Uppsala PRIVATE
    PICO_PRINTF_FLOAT_SUPPORT=1
    PICO_SCANF_FLOAT_SUPPORT=1
)

# pull in common dependencies
target_link_libraries(power_source_10A_Uppsala
	pico_stdlib
	pico_time
	hardware_timer
	hardware_uart
	hardware_irq
	hardware_pwm
	hardware_adc
	hardware_i2c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(power_source_10A_Uppsala 1)
pico_enable_stdio_uart(power_source_10A_Uppsala 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(power_source_10A_Uppsala)

set(OUTPUT_DIR "${PROJECT_SOURCE_DIR}/bin")
add_custom_command(
    TARGET power_source_10A_Uppsala
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE_DIR:power_source_10A_Uppsala>/power_source_10A_Uppsala.uf2"
        "${OUTPUT_DIR}/power_source_10A_Uppsala.uf2"
    COMMAND date "+%Y-%m-%d %H:%M:%S" > "${OUTPUT_DIR}/timestamp.txt"
    COMMENT "Copying UF2 and writing compilation timestamp"
)

