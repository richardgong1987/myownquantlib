# tests/CMakeLists.txt
#
# Links against myownql + Boost.Test.
# Each phase adds its own test file here.

find_package(Boost CONFIG REQUIRED COMPONENTS unit_test_framework)

add_executable(myownquantlib_tests
    test_main.cpp
    # Phase 1 tests will be added here, e.g.:
    # core/test_observer.cpp
    # core/test_lazyobject.cpp
)

target_link_libraries(myownquantlib_tests
    PRIVATE
        myownql
        Boost::unit_test_framework
)

target_apply_compiler_options(myownquantlib_tests)

# Register with CTest so `ctest` discovers and runs the binary
add_test(
    NAME myownquantlib_tests
    COMMAND myownquantlib_tests --log_level=message
)
