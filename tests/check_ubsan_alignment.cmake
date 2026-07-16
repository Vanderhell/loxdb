# SPDX-License-Identifier: MIT
if(NOT DEFINED CC OR NOT EXISTS "${CC}")
    find_program(UBSAN_CC NAMES clang clang.exe PATHS "C:/Program Files/LLVM/bin")
    if(NOT UBSAN_CC)
        message(STATUS "Skipping UBSan alignment gate: clang is not available")
        return()
    endif()
    set(CC "${UBSAN_CC}")
endif()

execute_process(
    COMMAND "${CC}" -std=c11 -O0 -g -fsanitize=undefined -fsanitize=alignment -I "${INCLUDE_DIR}" -c "${SRC}" -o "${OBJ}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr
)

if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "UBSan alignment gate failed to compile:\n${compile_stdout}\n${compile_stderr}")
endif()
