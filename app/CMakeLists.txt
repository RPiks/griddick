# UF2: core0 USB/log/KISS, core1 ADC+IFIR+mix+boxcar.

add_executable(griddick_tnc
    main.c
    radio_ipc.c
    core0_usb.c
    core1_radio.c
    rx_tract.c
    "${CMAKE_CURRENT_LIST_DIR}/../kiss/kiss.c"
    "${CMAKE_CURRENT_LIST_DIR}/../kiss/kiss_adc.c"
    "${CMAKE_CURRENT_LIST_DIR}/../kiss/kiss_tx.c"
    "${CMAKE_CURRENT_LIST_DIR}/../kiss/kiss_pwm.c"
    "${CMAKE_CURRENT_LIST_DIR}/../kiss/usb_descriptors.c"
    "${CMAKE_CURRENT_LIST_DIR}/../platform/gpio_ctrl.c"
    "${CMAKE_CURRENT_LIST_DIR}/../platform/cfg_flash.c"
    "${CMAKE_CURRENT_LIST_DIR}/../modem/src/ax25_hdlc.c"
    "${CMAKE_CURRENT_LIST_DIR}/../modem/src/ax25_fcs.c"
    "${CMAKE_CURRENT_LIST_DIR}/../modem/src/ax25_fix.c"
    "${CMAKE_CURRENT_LIST_DIR}/../test/awgn/awgn_pn.c"
    "${CMAKE_CURRENT_LIST_DIR}/../platform/sys_clock.c"
    "${CMAKE_CURRENT_LIST_DIR}/../platform/wall_time.c"
    "${CMAKE_CURRENT_LIST_DIR}/../platform/cpu_stat.c"
    "${CMAKE_CURRENT_LIST_DIR}/../log/log.c"
)

target_include_directories(griddick_tnc PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../kiss"
    "${CMAKE_CURRENT_LIST_DIR}/../platform"
    "${CMAKE_CURRENT_LIST_DIR}/../log"
    "${CMAKE_CURRENT_LIST_DIR}/../modem/include"
    "${CMAKE_CURRENT_LIST_DIR}/../modem/src"
    "${CMAKE_CURRENT_LIST_DIR}/../test/awgn"
)

target_compile_options(griddick_tnc PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:Release>:-O3>
)

pico_enable_stdio_usb(griddick_tnc 0)
pico_enable_stdio_uart(griddick_tnc 0)

set_property(TARGET griddick_tnc APPEND PROPERTY
    COMPILE_DEFINITIONS
        "USBD_PRODUCT=\"Griddick TNC\""
        SYS_CLOCK_TARGET_HZ=180000000
        SYS_CLOCK_VCO_HZ=1080000000
        SYS_CLOCK_PD1=3
        SYS_CLOCK_PD2=2
        PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
        PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=1
        PICO_USB_RESET_INCLUDE_DEFAULT_APP_DRIVER_CB=1
        PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
        PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=2
)

pico_set_program_name(griddick_tnc "Griddick TNC")
pico_set_program_description(griddick_tnc "AX.25 KISS simplex TNC")

target_link_libraries(griddick_tnc
    pico_stdlib
    pico_unique_id
    pico_bootrom
    pico_usb_reset
    tinyusb_device
    pico_multicore
    hardware_clocks
    hardware_pll
    hardware_gpio
    hardware_watchdog
    hardware_irq
    hardware_adc
    hardware_dma
    hardware_pwm
    hardware_flash
    pixdsp
)

pico_add_extra_outputs(griddick_tnc)

# build.version from manifest.yaml (repo root). buildUTC is the clock
# at this firmware build. The force file is removed after each stamp so
# the next build stamps again.
set(GRIDDICK_MANIFEST "${CMAKE_SOURCE_DIR}/manifest.yaml")
set(GRIDDICK_BUILD_HDR "${CMAKE_CURRENT_BINARY_DIR}/griddick_build.h")
set(GRIDDICK_BUILD_FORCE "${CMAKE_CURRENT_BINARY_DIR}/griddick_build.force")
if(NOT EXISTS "${GRIDDICK_MANIFEST}")
    message(FATAL_ERROR "firmware build needs ${GRIDDICK_MANIFEST}")
endif()
add_custom_command(
    OUTPUT "${GRIDDICK_BUILD_FORCE}"
    COMMAND ${CMAKE_COMMAND} -E touch "${GRIDDICK_BUILD_FORCE}"
    COMMENT "Mark firmware stamp dirty"
    VERBATIM
)
add_custom_command(
    OUTPUT "${GRIDDICK_BUILD_HDR}"
    COMMAND ${CMAKE_COMMAND}
        -DGRIDDICK_MANIFEST=${GRIDDICK_MANIFEST}
        -DGRIDDICK_OUT=${GRIDDICK_BUILD_HDR}
        -P ${CMAKE_CURRENT_LIST_DIR}/stamp_build.cmake
    COMMAND ${CMAKE_COMMAND} -E remove -f "${GRIDDICK_BUILD_FORCE}"
    DEPENDS "${GRIDDICK_BUILD_FORCE}"
            "${CMAKE_CURRENT_LIST_DIR}/stamp_build.cmake"
            "${GRIDDICK_MANIFEST}"
    COMMENT "Stamp firmware build identity"
    VERBATIM
)
add_custom_target(griddick_build_stamp DEPENDS "${GRIDDICK_BUILD_HDR}")
add_dependencies(griddick_tnc griddick_build_stamp)
set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/../kiss/kiss.c"
    APPEND PROPERTY OBJECT_DEPENDS "${GRIDDICK_BUILD_HDR}")
target_compile_definitions(griddick_tnc PRIVATE GRIDDICK_FW_STAMP=1)
