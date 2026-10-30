# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM12 LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
add_compile_options(-Wall -Wextra -Werror -Wpedantic)
include_directories("${LAMIX_ROOT}/include" "${LAMIX_ROOT}/kernel")
enable_testing()
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m12")

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/write_fixture.py"
          "${CMAKE_CURRENT_BINARY_DIR}/fixtures"
  COMMAND_ERROR_IS_FATAL ANY)

add_executable(buddy-native buddy_native.c
  "${LAMIX_ROOT}/kernel/pmm.c"
  "${LAMIX_ROOT}/lib/boot/contract.c"
  "${LAMIX_ROOT}/lib/boot/memory.c")
target_compile_options(buddy-native PRIVATE -fsanitize=address,undefined
  -fno-omit-frame-pointer)
target_link_options(buddy-native PRIVATE -fsanitize=address,undefined)
add_test(NAME m12-buddy-sanitize COMMAND buddy-native small.bin)
set_tests_properties(m12-buddy-sanitize PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures" TIMEOUT 180
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
add_test(NAME m12-buddy-large-sanitize COMMAND buddy-native large.bin)
set_tests_properties(m12-buddy-large-sanitize PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures" TIMEOUT 180
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
add_test(NAME m12-buddy-adjacent-sanitize COMMAND buddy-native small.bin)
set_tests_properties(m12-buddy-adjacent-sanitize PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures" TIMEOUT 180
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

add_test(NAME m12-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m12.py" -m "not qemu"
  "--junitxml=${LAMIX_ROOT}/out/m12/host.xml")
set_tests_properties(m12-host PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 300)

add_executable(buddy-bench buddy_bench.c
  "${LAMIX_ROOT}/kernel/pmm.c"
  "${LAMIX_ROOT}/lib/boot/contract.c"
  "${LAMIX_ROOT}/lib/boot/memory.c")
target_compile_options(buddy-bench PRIVATE -O2)
add_test(NAME m12-benchmark COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
  --benchmark "$<TARGET_FILE:buddy-bench>"
  --fixture "${CMAKE_CURRENT_BINARY_DIR}/fixtures/bench.bin"
  --output "${LAMIX_ROOT}/out/m12/benchmark.json")
set_tests_properties(m12-benchmark PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 180)

add_test(NAME m12-bios-uefi-guest COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m12.py" -m qemu
  "--junitxml=${LAMIX_ROOT}/out/m12/guest.xml")
set_tests_properties(m12-bios-uefi-guest PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 900
  ENVIRONMENT "LAMIX_STAGE2_BUILD=${LAMIX_ROOT}/build/m07-input;LAMIX_UEFI_BUILD=${LAMIX_ROOT}/build/m06")
