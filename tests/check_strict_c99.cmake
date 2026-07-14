# SPDX-License-Identifier: MIT
if(COMPILER_ID STREQUAL "MSVC")
    if(NOT DEFINED ENV{INCLUDE})
        find_program(STRICT_C99_CC NAMES clang clang.exe PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH)
        if(NOT STRICT_C99_CC)
            message(STATUS "Skipping strict C99 gate: neither the MSVC include environment nor clang are available")
            return()
        endif()
        execute_process(
            COMMAND "${STRICT_C99_CC}" -std=c99 -pedantic-errors -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror -I "${INCLUDE_DIR}" -c "${SRC}" -o "${OBJ}"
            RESULT_VARIABLE compile_result
            OUTPUT_VARIABLE compile_stdout
            ERROR_VARIABLE compile_stderr
        )
        if(NOT compile_result EQUAL 0)
            message(FATAL_ERROR "strict C99 gate failed:\n${compile_stdout}\n${compile_stderr}")
        endif()
        return()
    endif()
    execute_process(
        COMMAND "${CC}" /nologo /std:c11 /W4 /WX /permissive- /I "${INCLUDE_DIR}" /c "${SRC}" /Fo"${OBJ}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_stdout
        ERROR_VARIABLE compile_stderr
    )
else()
    execute_process(
        COMMAND "${CC}" -std=c99 -pedantic-errors -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror -I "${INCLUDE_DIR}" -c "${SRC}" -o "${OBJ}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_stdout
        ERROR_VARIABLE compile_stderr
    )
endif()

if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "strict C99 gate failed:\n${compile_stdout}\n${compile_stderr}")
endif()
