# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(LamixM01 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
find_program(NASM_EXECUTABLE nasm REQUIRED)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/entry.o"
  COMMAND "${NASM_EXECUTABLE}" --reproducible -f elf64 -o "${CMAKE_CURRENT_BINARY_DIR}/entry.o" entry.asm
  DEPENDS tests/toolchain/entry.asm
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/toolchain"
  VERBATIM)
add_executable(m01-smoke tests/toolchain/probe.c tests/toolchain/probe.cpp "${CMAKE_CURRENT_BINARY_DIR}/entry.o")
set_target_properties(m01-smoke PROPERTIES SUFFIX .elf C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO LINKER_LANGUAGE C)
target_compile_options(m01-smoke PRIVATE
  -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -mno-red-zone
  -fno-unwind-tables -fno-asynchronous-unwind-tables
  "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=/lamix"
  "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=/lamix/build"
  "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=/lamix"
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti;-fno-threadsafe-statics;-fno-use-cxa-atexit>
  $<$<CONFIG:Debug>:-O0;-g>
  $<$<CONFIG:Release>:-O2;-g0>)
target_link_options(m01-smoke PRIVATE -nostdlib -nostartfiles -nodefaultlibs -static -no-pie
  "-Wl,-T,${CMAKE_SOURCE_DIR}/tests/toolchain/link.ld" -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--fatal-warnings
  "-Wl,-Map,${CMAKE_BINARY_DIR}/m01-smoke.map")
set_property(TARGET m01-smoke APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/tests/toolchain/link.ld")
include(CTest)
if(BUILD_TESTING)
  add_test(NAME host-tests COMMAND "${Python3_EXECUTABLE}" -m pytest -q -m "not qemu" "${CMAKE_SOURCE_DIR}/tests")
  set_tests_properties(host-tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 120)
  add_test(NAME sample-elf COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/verify_elf.py" "${CMAKE_BINARY_DIR}")
  set_tests_properties(sample-elf PROPERTIES TIMEOUT 30)
endif()
