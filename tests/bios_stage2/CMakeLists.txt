# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(LamixStage2 LANGUAGES NONE)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(LAMIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
option(LAMIX_STAGE2_HOST_ONLY "Host parsers không cross/NASM/QEMU" OFF)
option(LAMIX_SANITIZERS "ASan/UBSan native host" OFF)
file(MAKE_DIRECTORY "${LAMIX_ROOT}/out/m05")
set(_profile "$<IF:$<CONFIG:Release>,Release,Debug>")
if(NOT LAMIX_STAGE2_HOST_ONLY)
  add_custom_target(stage2-target ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_gate.py"
      --build "${CMAKE_CURRENT_BINARY_DIR}" --profile "${_profile}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_stage2.py"
      --build "${CMAKE_CURRENT_BINARY_DIR}" --profile "${_profile}"
    WORKING_DIRECTORY "${LAMIX_ROOT}" VERBATIM)
endif()
enable_testing()
set(_native_args --build "${CMAKE_CURRENT_BINARY_DIR}/native-host" --profile "${_profile}")
if(LAMIX_SANITIZERS)
  list(APPEND _native_args --sanitize)
endif()
add_test(NAME stage2-native COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_host.py" ${_native_args})
set_tests_properties(stage2-native PROPERTIES TIMEOUT 90)
if(NOT LAMIX_SANITIZERS)
  add_test(NAME stage2-host COMMAND "${Python3_EXECUTABLE}" -m pytest -q "${CMAKE_CURRENT_SOURCE_DIR}/test_stage2_host.py"
    "--junitxml=${LAMIX_ROOT}/out/m05/host-${_profile}.xml")
  set_tests_properties(stage2-host PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}" TIMEOUT 90)
endif()
if(NOT LAMIX_STAGE2_HOST_ONLY)
  add_test(NAME stage2-build-gate COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_gate.py"
    --build "${CMAKE_CURRENT_BINARY_DIR}" --verify)
  add_test(NAME stage2-qemu COMMAND "${Python3_EXECUTABLE}" -m pytest -q "${CMAKE_CURRENT_SOURCE_DIR}/test_stage2_guest.py"
    "--junitxml=${LAMIX_ROOT}/out/m05/guest-${_profile}.xml")
  set_tests_properties(stage2-build-gate stage2-qemu PROPERTIES WORKING_DIRECTORY "${LAMIX_ROOT}"
    ENVIRONMENT "LAMIX_STAGE2_BUILD=${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(stage2-qemu PROPERTIES TIMEOUT 240)
endif()
