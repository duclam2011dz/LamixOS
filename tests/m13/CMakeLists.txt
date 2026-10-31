# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM13 LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
add_compile_options(-Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined
  -fno-omit-frame-pointer)
add_link_options(-fsanitize=address,undefined)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../kernel")
enable_testing()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../out/m13")
add_executable(m13-native native.c
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/acpi.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/pci.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/device.c")
add_test(NAME m13-native-sanitize COMMAND m13-native)
set_tests_properties(m13-native-sanitize PROPERTIES TIMEOUT 180
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
add_test(NAME m13-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m13.py" -m "not qemu"
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m13/host.xml")
set_tests_properties(m13-host PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 300)
add_test(NAME m13-bios-uefi-guest COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m13.py" -m qemu
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m13/guest.xml")
set_tests_properties(m13-bios-uefi-guest PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 900
  ENVIRONMENT "LAMIX_STAGE2_BUILD=${CMAKE_CURRENT_SOURCE_DIR}/../../build/m07-input;LAMIX_UEFI_BUILD=${CMAKE_CURRENT_SOURCE_DIR}/../../build/m06")
