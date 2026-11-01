# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM20 LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
add_compile_options(-Wall -Wextra -Werror -Wpedantic)
include_directories("${LAMIX_ROOT}/kernel")
enable_testing()
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m20")

add_executable(m20-native native.c "${LAMIX_ROOT}/kernel/process.c"
  "${LAMIX_ROOT}/kernel/vmm.c" "${LAMIX_ROOT}/kernel/scheduler.c")
target_compile_options(m20-native PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(m20-native PRIVATE -fsanitize=address,undefined)
add_test(NAME m20-native-sanitize COMMAND m20-native)
set_tests_properties(m20-native-sanitize PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}"
  TIMEOUT 180 ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

add_test(NAME m20-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m20.py" -m "not qemu"
  "--junitxml=${LAMIX_ROOT}/out/m20/host.xml")
add_test(NAME m20-bios-uefi-guest COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m20.py" -m qemu
  "--junitxml=${LAMIX_ROOT}/out/m20/guest.xml")
set_tests_properties(m20-host m20-bios-uefi-guest PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}"
  TIMEOUT 1800 ENVIRONMENT "LAMIX_STAGE2_BUILD=${LAMIX_ROOT}/build/m17-bios;LAMIX_UEFI_BUILD=${LAMIX_ROOT}/build/m17-uefi")
