# src/CMakeLists.txt
#
# Builds the myownql static library.
# Each phase of the learning plan adds its .cpp files here.
# Headers live under include/myownql/ and are accessed as <myownql/...>.

add_library(myownql STATIC
    # Phase 1: core infrastructure
    # core/errors.cpp        (to be added)
    # core/settings.cpp      (to be added)
    # ...

    # Placeholder so CMake accepts an empty library during early scaffolding.
    placeholder.cpp
)

target_include_directories(myownql
    PUBLIC
        # Consumers of myownql include headers as <myownql/foo.hpp>
        "${PROJECT_SOURCE_DIR}/include"
    PRIVATE
        # Implementation files include peers without the myownql/ prefix
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_apply_compiler_options(myownql)
