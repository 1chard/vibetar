cmake_minimum_required(VERSION 3.20)
project(xsm3_native_tests LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
get_filename_component(ROOT "../.." ABSOLUTE)
set(LIB "${ROOT}/third_party/libxsm3")
# Use the same generated SHA source compiled into the firmware.
if(NOT EXISTS "${ROOT}/build/generated/libxsm3/excrypt_sha.c")
  message(FATAL_ERROR "Build firmware first to prepare its SHA adapter")
endif()
add_executable(xsm3_native_tests main.cpp fixture.c
  "${ROOT}/src/xsm3_auth.cpp"
  "${LIB}/usbdsec.c" "${LIB}/excrypt_des.c" "${LIB}/excrypt_parve.c"
  "${ROOT}/build/generated/libxsm3/excrypt_sha.c")
target_include_directories(xsm3_native_tests PRIVATE stubs "${ROOT}/include" "${LIB}")
if(MSVC)
  target_compile_options(xsm3_native_tests PRIVATE /Zp1 /FI${CMAKE_CURRENT_SOURCE_DIR}/msvc_compat.h)
endif()
enable_testing()
add_test(NAME xsm3_session_crypto COMMAND xsm3_native_tests)
