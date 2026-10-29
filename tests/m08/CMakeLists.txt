# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM08 LANGUAGES NONE)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
enable_testing()
add_test(NAME m08-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m08.py" -m "not qemu"
  "--junitxml=${LAMIX_ROOT}/out/m08/host.xml")
add_test(NAME m08-bios-uefi-guest COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m08.py" -m qemu
  "--junitxml=${LAMIX_ROOT}/out/m08/guest.xml")
set_tests_properties(m08-host m08-bios-uefi-guest PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 900
  ENVIRONMENT "LAMIX_STAGE2_BUILD=${LAMIX_ROOT}/build/m07-input;LAMIX_UEFI_BUILD=${LAMIX_ROOT}/build/m06")
