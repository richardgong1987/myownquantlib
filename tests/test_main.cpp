// test_main.cpp
//
// Boost.Test entry point. The BOOST_TEST_MODULE macro defines the name of the
// root test suite and, combined with the header-only include below, generates
// the main() function automatically.
//
// Individual test cases will be added in separate files as each phase is
// implemented (e.g. core/test_observer.cpp, time/test_date.cpp, …).

#define BOOST_TEST_MODULE myownql_tests
#include <boost/test/included/unit_test.hpp>

// Placeholder: a single always-passing test so the binary links and runs.
BOOST_AUTO_TEST_CASE(scaffold_is_alive)
{
    BOOST_TEST(true);
}
