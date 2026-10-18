set(VW_CONTINUOUS 0 CACHE STRING "Experimental held-low draining: 0=edge, 1=level")
set(VW_RESPONSE_DELAY_US 10 CACHE STRING "Unmeasured response delay; validate on hardware")
set(VW_RESPONSE_TIMEOUT_US 500 CACHE STRING "Discard late requests after this interval")
add_executable(videowriter_usb main.c usb_input.c usb_descriptors.c
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c)
target_include_directories(videowriter_usb PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(videowriter_usb PRIVATE PIO_USB_USE_TINYUSB
    VW_CONTINUOUS=${VW_CONTINUOUS} VW_RESPONSE_DELAY_US=${VW_RESPONSE_DELAY_US}
    VW_RESPONSE_TIMEOUT_US=${VW_RESPONSE_TIMEOUT_US})
target_compile_options(videowriter_usb PRIVATE -Wall -Wextra)
target_link_libraries(videowriter_usb PRIVATE translator boot_keyboard videowriter
    pico_stdlib pico_multicore pico_unique_id hardware_uart pico_pio_usb tinyusb_host tinyusb_device)
pico_enable_stdio_uart(videowriter_usb 0)
pico_enable_stdio_usb(videowriter_usb 0)
set_target_properties(videowriter_usb PROPERTIES LINKER_LANGUAGE C)
pico_add_extra_outputs(videowriter_usb)
