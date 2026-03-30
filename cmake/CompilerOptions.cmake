# cmake/CompilerOptions.cmake
#
# Provides a single reusable function that applies strict, cross-platform
# compiler warnings and settings to any target.
#
# Usage:
#   target_apply_compiler_options(<target>)

function(target_apply_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4         # Warning level 4 (high)
            /WX         # Treat warnings as errors
            /DNOMINMAX  # Prevent windows.h from defining min/max macros
            /wd4100     # Suppress: unreferenced formal parameter
            /wd4127     # Suppress: conditional expression is constant
        )
    else()
        # GCC, Clang, AppleClang
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wno-unused-parameter
            -Wno-unknown-pragmas
        )
    endif()
endfunction()
