// main.cpp — myownquantlib demo
//
// This file grows alongside the implementation plan.
// Currently: Phase 0 scaffold — prints the library version and exits.

#include <myownql/version.hpp>
#include <iostream>

int main()
{
    std::cout << "myownquantlib v" << MYOWNQL_VERSION << "\n";
    std::cout << "Scaffold ready. Start Phase 1.\n";
    return 0;
}
