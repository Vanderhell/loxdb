# SPDX-License-Identifier: MIT
if(NOT DEFINED BUILD_DIR OR BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "BUILD_DIR is required")
endif()
if(NOT DEFINED INSTALL_PREFIX OR INSTALL_PREFIX STREQUAL "")
    message(FATAL_ERROR "INSTALL_PREFIX is required")
endif()
if(NOT DEFINED CONSUMER_SOURCE_DIR OR CONSUMER_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "CONSUMER_SOURCE_DIR is required")
endif()
if(NOT EXISTS "${CONSUMER_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Consumer source project is missing: ${CONSUMER_SOURCE_DIR}")
endif()
if(NOT DEFINED CONSUMER_BUILD_DIR OR CONSUMER_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "CONSUMER_BUILD_DIR is required")
endif()
if(NOT DEFINED CONSUMER_EXE_NAME OR CONSUMER_EXE_NAME STREQUAL "")
    message(FATAL_ERROR "CONSUMER_EXE_NAME is required")
endif()
if(NOT DEFINED INSTALL_CONFIG OR INSTALL_CONFIG STREQUAL "")
    set(INSTALL_CONFIG "Debug")
endif()
if(NOT DEFINED EXPECT_CONFIGURE_FAILURE)
    set(EXPECT_CONFIGURE_FAILURE 0)
endif()

file(REMOVE_RECURSE "${INSTALL_PREFIX}" "${CONSUMER_BUILD_DIR}")
file(MAKE_DIRECTORY "${INSTALL_PREFIX}")
file(MAKE_DIRECTORY "${CONSUMER_BUILD_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${INSTALL_CONFIG}" --target loxdb lox_json_wrapper lox_import_export
    RESULT_VARIABLE build_deps_rc
    OUTPUT_VARIABLE build_deps_out
    ERROR_VARIABLE build_deps_err
)
if(NOT build_deps_rc EQUAL 0)
    message(FATAL_ERROR "Dependency build failed:\n${build_deps_out}\n${build_deps_err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}" --config "${INSTALL_CONFIG}"
    RESULT_VARIABLE install_rc
    OUTPUT_VARIABLE install_out
    ERROR_VARIABLE install_err
)
if(NOT install_rc EQUAL 0)
    message(FATAL_ERROR "Install step failed:\n${install_out}\n${install_err}")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -S "${CONSUMER_SOURCE_DIR}"
        -B "${CONSUMER_BUILD_DIR}"
        -DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}
        -DCMAKE_BUILD_TYPE=${INSTALL_CONFIG}
    RESULT_VARIABLE configure_rc
    OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err
)

if(EXPECT_CONFIGURE_FAILURE)
    if(configure_rc EQUAL 0)
        message(FATAL_ERROR "Consumer configure unexpectedly succeeded:\n${configure_out}\n${configure_err}")
    endif()
    return()
endif()

if(NOT configure_rc EQUAL 0)
    message(FATAL_ERROR "Consumer configure failed:\n${configure_out}\n${configure_err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${CONSUMER_BUILD_DIR}" --config "${INSTALL_CONFIG}"
    RESULT_VARIABLE build_rc
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE build_err
)
if(NOT build_rc EQUAL 0)
    message(FATAL_ERROR "Consumer build failed:\n${build_out}\n${build_err}")
endif()

set(_exe "${CONSUMER_BUILD_DIR}/bin/${CONSUMER_EXE_NAME}")
if(WIN32)
    set(_exe "${CONSUMER_BUILD_DIR}/bin/${INSTALL_CONFIG}/${CONSUMER_EXE_NAME}.exe")
    if(NOT EXISTS "${_exe}")
        set(_exe "${CONSUMER_BUILD_DIR}/bin/${CONSUMER_EXE_NAME}.exe")
    endif()
endif()

if(NOT EXISTS "${_exe}")
    message(FATAL_ERROR "Consumer executable missing: ${_exe}")
endif()

execute_process(
    COMMAND "${_exe}"
    RESULT_VARIABLE run_rc
    OUTPUT_VARIABLE run_out
    ERROR_VARIABLE run_err
)
if(NOT run_rc EQUAL 0)
    message(FATAL_ERROR "Consumer run failed:\n${run_out}\n${run_err}")
endif()
