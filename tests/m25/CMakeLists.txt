# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM25 LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../kernel")
enable_testing()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../out/m25")

add_executable(m25-native native.c
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/block.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/virtio_blk.c")
target_link_libraries(m25-native PRIVATE Threads::Threads)
target_compile_options(m25-native PRIVATE -Wall -Wextra -Werror -Wpedantic
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(m25-native PRIVATE -fsanitize=address,undefined)
add_test(NAME m25-native-sanitize COMMAND m25-native)
set_tests_properties(m25-native-sanitize PROPERTIES TIMEOUT 180
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")

add_executable(m25-concurrency concurrency.c
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/block.c")
target_link_libraries(m25-concurrency PRIVATE Threads::Threads)
target_compile_options(m25-concurrency PRIVATE -Wall -Wextra -Werror -Wpedantic
  -fsanitize=thread -fno-omit-frame-pointer)
target_link_options(m25-concurrency PRIVATE -fsanitize=thread)
add_test(NAME m25-concurrency-tsan COMMAND m25-concurrency)
set_tests_properties(m25-concurrency-tsan PROPERTIES TIMEOUT 180
  ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")

add_test(NAME m25-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m25_script.py" -m "not qemu"
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m25/host.xml")
set_tests_properties(m25-host PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 600)

add_test(NAME m25-guest-bios-uefi COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m25.py" -m qemu
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m25/guest.xml")
set_tests_properties(m25-guest-bios-uefi PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 3600)
