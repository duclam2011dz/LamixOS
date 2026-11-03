# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM27 LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../user/include"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel")
enable_testing()
add_executable(m27-native native.c
  "${CMAKE_CURRENT_SOURCE_DIR}/../../user/shell.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../user/services.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/vfs.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/fat32.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/block.c")
target_compile_options(m27-native PRIVATE -Wall -Wextra -Werror -Wpedantic
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(m27-native PRIVATE -fsanitize=address,undefined)
add_test(NAME m27-native-sanitize COMMAND m27-native)
set_tests_properties(m27-native-sanitize PROPERTIES TIMEOUT 180
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
add_test(NAME m27-docs COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/validate_docs.py")
set_tests_properties(m27-docs PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 180)
add_test(NAME m27-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m27.py" -m "not qemu"
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m27/host.xml")
set_tests_properties(m27-host PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 600)
add_test(NAME m27-guest-bios-uefi COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m27.py" -m qemu
  "--junitxml=${CMAKE_CURRENT_SOURCE_DIR}/../../out/m27/guest.xml")
set_tests_properties(m27-guest-bios-uefi PROPERTIES WORKING_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/../.." TIMEOUT 7200)
