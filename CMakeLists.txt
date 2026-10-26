# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.20)

# Use the W pin definitions/libraries; physical Pico/Pico W is detected at boot.
set(PICO_BOARD pico_w CACHE STRING "SDK target for universal RP2040 firmware" FORCE)
if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    message(FATAL_ERROR "Set PICO_SDK_PATH to a Pico SDK 2.2.0 checkout with its TinyUSB submodule")
endif()
include("${PICO_SDK_PATH}/pico_sdk_init.cmake")
project(vibetar VERSION 0.1.0 LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Both supported boards have 2 MiB. Keep bonds separate from guitar settings.
set(PICO_FLASH_SIZE_BYTES "(2 * 1024 * 1024) - 16384")
pico_sdk_init()
# SDK 2.2.0 HIDS ignores blob offsets and prepared writes. Use the local adapter.
get_target_property(btstack_ble_sources pico_btstack_ble INTERFACE_SOURCES)
list(FILTER btstack_ble_sources EXCLUDE REGEX "/hids_device\\.c$")
list(FILTER btstack_ble_sources EXCLUDE REGEX "/hids_client\\.c$")
set_property(TARGET pico_btstack_ble PROPERTY INTERFACE_SOURCES "${btstack_ble_sources}")

if(NOT PICO_SDK_VERSION_STRING VERSION_EQUAL "2.2.0")
    message(WARNING "Validated with Pico SDK 2.2.0 / bundled TinyUSB 0.18.0; check custom driver API when upgrading")
endif()

set(XSM3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libxsm3")
if(NOT EXISTS "${XSM3_DIR}/xsm3.c")
    message(FATAL_ERROR "Run git submodule update --init --recursive before building")
endif()
# This pinned upstream revision declares 'state' twice in ExCryptSha.
# Remove its unused initial declaration in a generated copy, preserving the submodule.
file(READ "${XSM3_DIR}/excrypt_sha.c" xsm3_sha_source)
set(xsm3_sha_unused "  uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};")
string(FIND "${xsm3_sha_source}" "${xsm3_sha_unused}" xsm3_sha_patch_position)
if(xsm3_sha_patch_position LESS 0)
    message(FATAL_ERROR "libxsm3 SHA adapter expects pinned revision a085709; review before updating")
endif()
string(REPLACE "${xsm3_sha_unused}" "" xsm3_sha_source "${xsm3_sha_source}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/libxsm3")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/libxsm3/excrypt_sha.c" "${xsm3_sha_source}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${XSM3_DIR}/excrypt_sha.c")
add_library(guitar_xsm3 STATIC
    src/xsm3_port.c
    "${XSM3_DIR}/usbdsec.c"
    "${XSM3_DIR}/excrypt_des.c"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/libxsm3/excrypt_sha.c"
    "${XSM3_DIR}/excrypt_parve.c")
target_include_directories(guitar_xsm3 PRIVATE include "${XSM3_DIR}")
target_link_libraries(guitar_xsm3 PRIVATE pico_rand)
# Include-based port fixes reset/random/error reporting without modifying the submodule.
set_property(SOURCE src/xsm3_port.c APPEND PROPERTY OBJECT_DEPENDS "${XSM3_DIR}/xsm3.c")

set(controller_sources
    src/main.cpp
    src/input.cpp
    src/outputs.cpp
    src/battery.cpp
    src/radio.cpp
    src/adxl345.cpp
    src/usb_device.cpp
    src/usb_descriptors.cpp
    src/xinput_driver.cpp
    src/xsm3_auth.cpp
    src/settings.cpp
    tests/logic_tests.cpp
    tests/settings_tests.cpp
    tests/xinput_profile_tests.cpp
    tests/boot_gesture_tests.cpp
    tests/firmware_control_tests.cpp
    tests/gpio_report_tests.cpp
    tests/extras_tests.cpp
    tests/tilt_tests.cpp
    tests/host_policy_tests.cpp
    tests/outputs_tests.cpp
    tests/battery_tests.cpp
    tests/bluetooth_tests.cpp
    tests/receiver_protocol_tests.cpp
    tests/xsm3_tests.cpp
)
foreach(target IN ITEMS vibetar vibetar-receiver)
    if(target STREQUAL "vibetar-receiver")
        set(role 1)
        set(bt_sources src/bluetooth_receiver.cpp src/hids_client_diagnostics.c)
        set(gatt receiver_ble)
    else()
        set(role 0)
        set(bt_sources src/bluetooth.cpp src/hids_device_features.c)
        set(gatt guitar_ble)
    endif()
    add_executable(${target} ${controller_sources} ${bt_sources})
    target_include_directories(${target} PRIVATE include)
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Werror>)
    target_compile_definitions(${target} PRIVATE VIBETAR_RECEIVER=${role}
        PICO_FLASH_BANK_STORAGE_OFFSET=0x1FC000 PICO_FLASH_BANK_TOTAL_SIZE=8192
        PICO_FLASH_ASSUME_CORE1_SAFE=1 PICO_STACK_SIZE=4096)
    target_link_libraries(${target} PRIVATE guitar_xsm3 pico_stdlib pico_bootrom pico_unique_id
        hardware_adc hardware_pwm hardware_i2c hardware_flash hardware_watchdog pico_flash tinyusb_device
        pico_cyw43_arch_none pico_btstack_ble pico_btstack_cyw43)
    pico_btstack_make_gatt_header(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/${gatt}.gatt")
    pico_enable_stdio_usb(${target} 0)
    pico_enable_stdio_uart(${target} 0)
    pico_add_extra_outputs(${target})
endforeach()
