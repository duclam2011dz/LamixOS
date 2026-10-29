# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixM07 LANGUAGES NONE)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
enable_testing()
add_test(NAME m07-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m07.py" -m "not qemu"
  "--junitxml=${LAMIX_ROOT}/out/m07/host.xml")
add_test(NAME m07-boot-parity COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_m07.py" -m qemu
  "--junitxml=${LAMIX_ROOT}/out/m07/guest.xml")
set_tests_properties(m07-host m07-boot-parity PROPERTIES
  WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 360
  ENVIRONMENT "LAMIX_STAGE2_BUILD=${LAMIX_ROOT}/build/m07-input;LAMIX_UEFI_BUILD=${LAMIX_ROOT}/build/m06")
