# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM26 LANGUAGES C)
find_package(Threads REQUIRED)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../kernel")
enable_testing()
add_executable(m26-native native.c
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/vfs.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/fat32.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/block.c")
target_link_libraries(m26-native PRIVATE Threads::Threads)
target_compile_options(m26-native PRIVATE -Wall -Wextra -Werror -Wpedantic
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(m26-native PRIVATE -fsanitize=address,undefined)
add_test(NAME m26-native-sanitize COMMAND m26-native)
set_tests_properties(m26-native-sanitize PROPERTIES TIMEOUT 180
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
add_executable(m26-concurrency concurrency.c
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/vfs.c")
target_link_libraries(m26-concurrency PRIVATE Threads::Threads)
target_compile_options(m26-concurrency PRIVATE -Wall -Wextra -Werror -Wpedantic
  -fsanitize=thread -fno-omit-frame-pointer)
target_link_options(m26-concurrency PRIVATE -fsanitize=thread)
add_test(NAME m26-concurrency-tsan COMMAND m26-concurrency)
set_tests_properties(m26-concurrency-tsan PROPERTIES TIMEOUT 180
  ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")
add_test(NAME m26-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m26_script.py"
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m26/host.xml")
set_tests_properties(m26-host PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 600)
add_test(NAME m26-guest-bios-uefi COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m26.py" -m qemu
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m26/guest.xml")
set_tests_properties(m26-guest-bios-uefi PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 7200)
