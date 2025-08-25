cmake_minimum_required(VERSION 3.13)
project(power_source_10A_Uppsala C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(power_source_10A_Uppsala
    ${CMAKE_CURRENT_LIST_DIR}/source/power_source.c
    ${CMAKE_CURRENT_LIST_DIR}/source/serial_transmission.c
)

# pull in common dependencies
target_link_libraries(power_source_10A_Uppsala
	pico_stdlib
	pico_time
	hardware_timer
	hardware_uart
	hardware_irq
	hardware_pwm
)

# enable usb output, disable uart output
pico_enable_stdio_usb(power_source_10A_Uppsala 1)
pico_enable_stdio_uart(power_source_10A_Uppsala 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(power_source_10A_Uppsala)


