# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixHarness LANGUAGES NONE)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
enable_testing()
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m02")
add_test(NAME harness-fixtures COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_harness.py" "--junitxml=${LAMIX_ROOT}/out/m02/fixtures.xml")
set_tests_properties(harness-fixtures PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 60)
add_test(NAME emulator-launch COMMAND "${Python3_EXECUTABLE}" -m pytest -q
  "${CMAKE_CURRENT_SOURCE_DIR}/test_launch.py" "--junitxml=${LAMIX_ROOT}/out/m02/launch.xml")
set_tests_properties(emulator-launch PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 40)
foreach(PROFILE bios uefi)
  add_test(NAME boot-${PROFILE} COMMAND "${Python3_EXECUTABLE}" "${LAMIX_ROOT}/scripts/run_qemu.py" --profile ${PROFILE})
  set_tests_properties(boot-${PROFILE} PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}" SKIP_RETURN_CODE 77 TIMEOUT 40)
endforeach()
