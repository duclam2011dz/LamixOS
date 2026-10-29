# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM09 LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
add_compile_options(-Wall -Wextra -Werror -Wpedantic)
include_directories("${LAMIX_ROOT}/include" "${LAMIX_ROOT}/kernel")
enable_testing()
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m09")

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/write_fixture.py"
          "${CMAKE_CURRENT_BINARY_DIR}/bootinfo.bin"
  COMMAND_ERROR_IS_FATAL ANY)

add_executable(pmm-native pmm_native.c
  "${LAMIX_ROOT}/kernel/pmm.c"
  "${LAMIX_ROOT}/lib/boot/contract.c"
  "${LAMIX_ROOT}/lib/boot/memory.c")
target_compile_options(pmm-native PRIVATE -fsanitize=address,undefined
  -fno-omit-frame-pointer)
target_link_options(pmm-native PRIVATE -fsanitize=address,undefined)
add_test(NAME m09-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m09.py" -m "not qemu"
  "--junitxml=${LAMIX_ROOT}/out/m09/host.xml")
add_test(NAME m09-pmm-sanitize COMMAND pmm-native
  "${CMAKE_CURRENT_BINARY_DIR}/bootinfo.bin")
set_tests_properties(m09-host m09-pmm-sanitize PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 120
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

add_test(NAME m09-bios-uefi-guest COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m09.py" -m qemu
  "--junitxml=${LAMIX_ROOT}/out/m09/guest.xml")
set_tests_properties(m09-bios-uefi-guest PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 600
  ENVIRONMENT "LAMIX_STAGE2_BUILD=${LAMIX_ROOT}/build/m07-input;LAMIX_UEFI_BUILD=${LAMIX_ROOT}/build/m06")
