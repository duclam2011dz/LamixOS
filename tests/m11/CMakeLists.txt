# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM11 LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
add_compile_options(-Wall -Wextra -Werror -Wpedantic)
include_directories("${LAMIX_ROOT}/kernel")
enable_testing()
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m11")

add_executable(heap-native heap_native.c "${LAMIX_ROOT}/kernel/heap.c")
target_compile_options(heap-native PRIVATE -fsanitize=address,undefined
  -fno-omit-frame-pointer)
target_link_options(heap-native PRIVATE -fsanitize=address,undefined)
add_test(NAME m11-heap-sanitize COMMAND heap-native)
set_tests_properties(m11-heap-sanitize PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 120
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

add_test(NAME m11-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m11.py" -m "not qemu"
  "--junitxml=${LAMIX_ROOT}/out/m11/host.xml")
add_test(NAME m11-bios-uefi-guest COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m11.py" -m qemu
  "--junitxml=${LAMIX_ROOT}/out/m11/guest.xml")
set_tests_properties(m11-host m11-bios-uefi-guest PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 900
  ENVIRONMENT "LAMIX_STAGE2_BUILD=${LAMIX_ROOT}/build/m07-input;LAMIX_UEFI_BUILD=${LAMIX_ROOT}/build/m06")
