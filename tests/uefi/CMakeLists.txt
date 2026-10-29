# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixUefi LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m06")
enable_testing()
add_executable(uefi-policy test_policy.c)
target_include_directories(uefi-policy PRIVATE "${LAMIX_ROOT}/boot/uefi"
                                                "${LAMIX_ROOT}/include")
target_compile_options(uefi-policy PRIVATE -Wall -Wextra -Werror -Wpedantic)
add_test(NAME uefi-policy COMMAND uefi-policy)
add_test(NAME uefi-pe COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_uefi.py"
  --build-only "${LAMIX_ROOT}/build/m06")
set_tests_properties(uefi-pe PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 120)
add_test(NAME uefi-guest COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_uefi.py"
  "--junitxml=${LAMIX_ROOT}/out/m06/guest.xml")
set_tests_properties(uefi-guest PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}"
  ENVIRONMENT "LAMIX_STAGE2_BUILD=${LAMIX_ROOT}/build/m05;LAMIX_UEFI_BUILD=${LAMIX_ROOT}/build/m06"
  TIMEOUT 300)
