# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixBiosStage1 LANGUAGES NONE)
find_program(NASM_EXECUTABLE nasm REQUIRED)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/stage1.bin" "${CMAKE_CURRENT_BINARY_DIR}/stage1.lst"
  COMMAND "${NASM_EXECUTABLE}" --reproducible -f bin
    -l "${CMAKE_CURRENT_BINARY_DIR}/stage1.lst"
    -o "${CMAKE_CURRENT_BINARY_DIR}/stage1.bin" stage1.asm
  DEPENDS "${LAMIX_ROOT}/boot/bios/stage1.asm"
  WORKING_DIRECTORY "${LAMIX_ROOT}/boot/bios"
  VERBATIM)
add_custom_target(stage1 ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stage1.bin")
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/probe.bin"
  COMMAND "${NASM_EXECUTABLE}" --reproducible -f bin
    -o "${CMAKE_CURRENT_BINARY_DIR}/probe.bin" stage_probe.asm
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/stage_probe.asm"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
add_custom_target(probe ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/probe.bin")
add_dependencies(probe stage1)
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m04")
enable_testing()
add_test(NAME stage1-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_host.py" "--junitxml=${LAMIX_ROOT}/out/m04/host.xml")
add_test(NAME stage1-qemu COMMAND "${Python3_EXECUTABLE}" -m pytest -q --maxfail=1
  "${CMAKE_CURRENT_SOURCE_DIR}/test_guest.py" "--junitxml=${LAMIX_ROOT}/out/m04/guest.xml")
set_tests_properties(stage1-host stage1-qemu PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}"
  ENVIRONMENT "LAMIX_STAGE1_BIN=${CMAKE_CURRENT_BINARY_DIR}/stage1.bin")
set_tests_properties(stage1-host PROPERTIES TIMEOUT 60)
set_tests_properties(stage1-qemu PROPERTIES TIMEOUT 240)
