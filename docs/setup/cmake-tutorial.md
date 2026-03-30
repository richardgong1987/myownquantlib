# CMake & vcpkg Setup — Step-by-Step Tutorial

This document explains every decision made when setting up the build system for `myownquantlib`. Read it alongside the actual files in the repository. Each section maps to a concrete file and explains what CMake concept is being used, why it exists, and what would break without it.

---

## Table of Contents

1. [What is CMake and why use it?](#1-what-is-cmake-and-why-use-it)
2. [What is vcpkg and how does it connect to CMake?](#2-what-is-vcpkg-and-how-does-it-connect-to-cmake)
3. [`cmake_minimum_required` — setting a floor](#3-cmake_minimum_required--setting-a-floor)
4. [`project()` — naming the build](#4-project--naming-the-build)
5. [C++ standard settings](#5-c-standard-settings)
6. [Default build type](#6-default-build-type)
7. [Module path and `include()`](#7-module-path-and-include)
8. [`add_library()` — building the core library](#8-add_library--building-the-core-library)
9. [`target_include_directories()` — telling the compiler where headers live](#9-target_include_directories--telling-the-compiler-where-headers-live)
10. [`PUBLIC`, `PRIVATE`, `INTERFACE` — visibility keywords](#10-public-private-interface--visibility-keywords)
11. [`target_link_libraries()` — connecting libraries together](#11-target_link_libraries--connecting-libraries-together)
12. [`add_executable()` — the demo binary](#12-add_executable--the-demo-binary)
13. [`add_subdirectory()` — splitting CMake across folders](#13-add_subdirectory--splitting-cmake-across-folders)
14. [`find_package()` — consuming third-party libraries](#14-find_package--consuming-third-party-libraries)
15. [CTest — the test runner](#15-ctest--the-test-runner)
16. [Reusable compiler options via a CMake function](#16-reusable-compiler-options-via-a-cmake-function)
17. [`message()` — the build summary](#17-message--the-build-summary)
18. [vcpkg.json — the dependency manifest](#18-vcpkgjson--the-dependency-manifest)
19. [The full build workflow](#19-the-full-build-workflow)
20. [Directory layout explained](#20-directory-layout-explained)

---

## 1. What is CMake and why use it?

A C++ compiler (`clang++`, `g++`) only knows how to turn one `.cpp` file into an object file, and one set of object files into a binary. It has no concept of a *project* — no idea which files to include, which libraries to link, or which flags to apply.

**CMake** is a *build system generator*. You describe your project in `CMakeLists.txt` files using a high-level language (CMake's own scripting language), and CMake generates the actual build files for whatever system you are on:

- On macOS / Linux → it generates `Makefile`s or Ninja build files
- On Windows → it generates Visual Studio `.sln` / `.vcxproj` files

You never edit the generated files. You only edit `CMakeLists.txt` and re-run CMake when the project structure changes.

**The configure + build separation:**

```
cmake -B build   ← "configure" step: reads CMakeLists.txt, generates build files into build/
cmake --build build  ← "build" step: invokes the generated build system (make, ninja, etc.)
```

This two-step design means the same `CMakeLists.txt` works on every platform and IDE.

---

## 2. What is vcpkg and how does it connect to CMake?

C++ has no standard package manager. `vcpkg` fills that gap. It:

1. Downloads the source code of libraries you request (e.g. Boost.Test)
2. Compiles them for your platform and architecture
3. Installs the compiled binaries and headers into a local `vcpkg_installed/` folder
4. Tells CMake where to find them via a **toolchain file**

The connection between vcpkg and CMake is the toolchain file:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

`CMAKE_TOOLCHAIN_FILE` is a special CMake variable that is loaded very early in the configure step, before any `find_package()` calls. The vcpkg toolchain file teaches CMake to look inside `vcpkg_installed/` when searching for packages.

**Manifest mode** means vcpkg reads `vcpkg.json` to know which packages to install. If they are not already cached, vcpkg compiles them automatically during `cmake -B build`. You never call `vcpkg install` manually.

---

## 3. `cmake_minimum_required` — setting a floor

```cmake
cmake_minimum_required(VERSION 3.21)
```

**What it does:** Declares the oldest version of CMake that can configure this project.

**Why it matters:** CMake's behaviour and the commands available change between versions. Features we use (like `CTest` integration and the `CONFIG` mode of `find_package`) require at least version 3.14. Setting a minimum version ensures that:

1. An old CMake will print a clear error instead of silently producing a broken build.
2. CMake applies the *policy* defaults from that version (CMake's way of maintaining backward compatibility while evolving).

We chose 3.21 because that version introduced `cmake_minimum_required(VERSION x.y...z)` policy ranges, and is comfortably available on all modern systems (macOS ships with 3.28+ via Homebrew).

---

## 4. `project()` — naming the build

```cmake
project(
    myownquantlib
    VERSION 0.1.0
    DESCRIPTION "Rebuilding QuantLib from scratch — a C++ learning project"
    LANGUAGES CXX
)
```

**What it does:** Defines the project. CMake sets several variables automatically:

| Variable | Value |
|---|---|
| `PROJECT_NAME` | `myownquantlib` |
| `PROJECT_VERSION` | `0.1.0` |
| `PROJECT_VERSION_MAJOR` | `0` |
| `PROJECT_VERSION_MINOR` | `1` |
| `PROJECT_VERSION_PATCH` | `0` |
| `PROJECT_SOURCE_DIR` | Absolute path to the folder containing this `CMakeLists.txt` |
| `PROJECT_BINARY_DIR` | Absolute path to the build folder (`build/`) |

`LANGUAGES CXX` tells CMake to find a C++ compiler and skip looking for a C compiler. Without this, CMake would also search for `cc` / `gcc`, which is unnecessary overhead.

**Why `VERSION` matters:** The version number flows into the build summary (`message(STATUS "myownquantlib ${PROJECT_VERSION}")`) and can be injected into headers with `configure_file()` in the future.

---

## 5. C++ standard settings

```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

**`CMAKE_CXX_STANDARD 17`** — Request C++17. CMake translates this into `-std=c++17` on GCC/Clang or `/std:c++17` on MSVC.

**`CMAKE_CXX_STANDARD_REQUIRED ON`** — If the compiler does not support C++17, fail loudly. Without this, CMake silently falls back to C++14 or C++11, and your code breaks in confusing ways at compile time.

**`CMAKE_CXX_EXTENSIONS OFF`** — Disable compiler-specific extensions. GCC's default is `-std=gnu++17`, which enables GNU extensions on top of standard C++. We want strict standard compliance (`-std=c++17`) because:

- Extensions make code non-portable to MSVC or Clang
- They can silently accept non-standard code that fails on stricter compilers
- We are learning the language, not compiler quirks

These three lines are *global defaults*. Every target created after this point inherits them unless it overrides explicitly.

---

## 6. Default build type

```cmake
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
```

**What it does:** If the user did not specify `-DCMAKE_BUILD_TYPE=...`, default to `Release`.

**Why it is needed:** CMake has no built-in default build type for single-config generators (the kind used on macOS/Linux with Makefiles or Ninja). Without this, `CMAKE_BUILD_TYPE` is an empty string, which means:
- No optimization flags are added (`-O2`, `-O3`)
- No debug info is generated
- Confusing: the build is neither optimized nor debuggable

Common values:

| Value | Flags (GCC/Clang) | Use case |
|---|---|---|
| `Release` | `-O3 -DNDEBUG` | Production, benchmarks |
| `Debug` | `-g -O0` | Step-through debugging |
| `RelWithDebInfo` | `-O2 -g -DNDEBUG` | Production + crash analysis |
| `MinSizeRel` | `-Os -DNDEBUG` | Embedded / size-constrained |

The `CACHE STRING "Build type" FORCE` stores the value in `build/CMakeCache.txt` so it persists across re-runs of `cmake -B build`.

---

## 7. Module path and `include()`

```cmake
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerOptions)
```

**`CMAKE_MODULE_PATH`** is a list of directories CMake searches when you call `include(SomeName)` or `find_package(SomeName MODULE)`. By appending our `cmake/` folder, we can write reusable helper `.cmake` files and include them by name.

**`include(CompilerOptions)`** loads `cmake/CompilerOptions.cmake` — which defines our `target_apply_compiler_options()` function. This is similar to `#include` in C++: the file's content is copied into the current CMake scope.

The pattern of putting helpers in `cmake/` and loading them with `include()` keeps the root `CMakeLists.txt` clean and modular.

---

## 8. `add_library()` — building the core library

```cmake
# In src/CMakeLists.txt
add_library(myownql STATIC
    placeholder.cpp
)
```

**What it does:** Creates a *target* named `myownql` and specifies that it should be compiled into a static library (`libmyownql.a` on macOS/Linux, `myownql.lib` on Windows).

**`STATIC` vs `SHARED`:**

| Type | Output | Linking |
|---|---|---|
| `STATIC` | `libmyownql.a` | Compiled directly into every consumer |
| `SHARED` | `libmyownql.dylib` / `.so` | Loaded at runtime; one copy in memory |

We use `STATIC` because:
- No need to worry about deployment (`dylib` must be findable at runtime)
- Simpler for a learning project
- Equivalent to how QuantLib itself is typically built

A target is CMake's central abstraction. Everything that follows — include paths, link libraries, compiler options — is attached to targets, not to global variables.

---

## 9. `target_include_directories()` — telling the compiler where headers live

```cmake
target_include_directories(myownql
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

When a `.cpp` file writes `#include <myownql/version.hpp>`, the compiler needs to know which directory to prepend when searching for `myownql/version.hpp`. That directory must be listed in the include path (`-I` flag on GCC/Clang).

This command registers two directories:

1. **`${PROJECT_SOURCE_DIR}/include`** (PUBLIC) — the `include/` folder at the project root, where public headers live. This path is exported to anything that links against `myownql`.

2. **`${CMAKE_CURRENT_SOURCE_DIR}`** (PRIVATE) — the `src/` folder. Implementation `.cpp` files can `#include "core/errors.hpp"` without repeating the `src/` prefix. Not exported to consumers.

The difference between the two is covered in the next section.

---

## 10. `PUBLIC`, `PRIVATE`, `INTERFACE` — visibility keywords

This is one of the most important CMake concepts. Every property on a target (include dirs, link libraries, compile flags) has a **propagation scope**:

| Keyword | Who sees it |
|---|---|
| `PRIVATE` | Only this target's own compilation |
| `PUBLIC` | This target **and** every target that links against it |
| `INTERFACE` | Only targets that link against this target (not this target itself) |

**Example with our project:**

```
myownql (library)
  PUBLIC include dir: include/
  PRIVATE include dir: src/

myownquantlib_demo (executable)
  links against: myownql
```

Because `include/` is `PUBLIC` on `myownql`, the demo executable automatically has `include/` on its include path without needing its own `target_include_directories`. It inherits the property transitively.

Because `src/` is `PRIVATE`, the demo executable does *not* get `src/` on its path — only `myownql`'s own `.cpp` files do.

**Why this matters:** In large projects with many libraries depending on each other, `PUBLIC` vs `PRIVATE` prevents include-path leakage. If library A's internal headers are `PRIVATE`, library B that uses A cannot accidentally include A's internals.

---

## 11. `target_link_libraries()` — connecting libraries together

```cmake
# In CMakeLists.txt (root)
target_link_libraries(myownquantlib_demo PRIVATE myownql)

# In tests/CMakeLists.txt
target_link_libraries(myownquantlib_tests
    PRIVATE
        myownql
        Boost::unit_test_framework
)
```

**What it does:** Tells the linker to include the symbols from the listed libraries when building the target.

**Modern CMake vs old CMake:** In old CMake (pre-3.0), you would write:
```cmake
link_directories(/path/to/boost/lib)   # fragile global state
target_link_libraries(mytarget boost_test)  # just a raw name
```

In modern CMake, `Boost::unit_test_framework` is an *imported target* — a CMake object that carries all the information needed to use Boost.Test: the library path, the include paths, any required compile definitions. Linking against an imported target transitively applies everything.

**`PRIVATE` here** means: the demo and test binaries need Boost to build and link, but they do not re-export it to others. Since executables have no consumers, it is always `PRIVATE`.

---

## 12. `add_executable()` — the demo binary

```cmake
add_executable(myownquantlib_demo main.cpp)
target_link_libraries(myownquantlib_demo PRIVATE myownql)
target_apply_compiler_options(myownquantlib_demo)
```

Creates an executable target. Same target system as `add_library()`: you attach include dirs, link libraries, and compile options to the target name.

`target_apply_compiler_options` is our custom function (defined in `cmake/CompilerOptions.cmake`) — it calls `target_compile_options()` under the hood. This is described in section 16.

---

## 13. `add_subdirectory()` — splitting CMake across folders

```cmake
# In root CMakeLists.txt
add_subdirectory(src)
add_subdirectory(tests)
```

**What it does:** Tells CMake to process the `CMakeLists.txt` inside the named folder. The child `CMakeLists.txt` has its own scope for variables, but all *targets* are global — a target defined in `src/CMakeLists.txt` can be linked against from `tests/CMakeLists.txt`.

This is how CMake projects scale. Each folder owns its own `CMakeLists.txt` and declares what it builds. The root file just orchestrates the order.

**`CMAKE_CURRENT_SOURCE_DIR`** inside a child `CMakeLists.txt` always points to that child's directory (`src/`, `tests/`), not the root. This makes paths portable.

---

## 14. `find_package()` — consuming third-party libraries

```cmake
# In tests/CMakeLists.txt
find_package(Boost CONFIG REQUIRED COMPONENTS unit_test_framework)
```

**What it does:** Searches for an installed library and imports it as CMake targets.

**`CONFIG` mode** tells CMake to find and load the library's own CMake config file (e.g. `BoostConfig.cmake`). This is the modern, preferred way. The alternative (`MODULE` mode) uses a CMake-bundled `FindBoost.cmake` script, which is older and less reliable — hence the `CMP0167` warning we eliminated.

**`REQUIRED`** makes CMake fail with an error if Boost is not found, instead of silently continuing with a broken build.

**`COMPONENTS unit_test_framework`** requests only the specific Boost sub-library we need. Boost has 180+ independent libraries; requesting only what we use keeps compile time minimal.

**How vcpkg connects to this:** The vcpkg toolchain file (loaded via `CMAKE_TOOLCHAIN_FILE`) prepends `vcpkg_installed/arm64-osx/share/` to CMake's package search paths. That folder contains `boost/BoostConfig.cmake`, which is what `find_package` loads. Without the toolchain file, CMake would search standard system paths and likely not find anything.

---

## 15. CTest — the test runner

```cmake
# In root CMakeLists.txt
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# In tests/CMakeLists.txt
add_test(
    NAME myownquantlib_tests
    COMMAND myownquantlib_tests --log_level=message
)
```

**`include(CTest)`** activates CMake's built-in test infrastructure and creates the `BUILD_TESTING` option (defaults to `ON`). It also sets up `ctest` to work with the build tree.

**`add_test(NAME ... COMMAND ...)`** registers a test with CTest. The `NAME` is what `ctest` displays; the `COMMAND` is the executable to run (plus optional arguments). CMake knows where to find `myownquantlib_tests` because it's a target in the same build.

Running tests:

```bash
ctest --test-dir build --output-on-failure
# Or directly:
./build/myownquantlib_tests --run_test=SuiteName/TestName
```

**Why `BUILD_TESTING` guards the `add_subdirectory`:** Tests add compile time. In CI or production builds where tests will be run separately, you might configure with `-DBUILD_TESTING=OFF` to skip them. The guard makes this opt-out explicit.

---

## 16. Reusable compiler options via a CMake function

```cmake
# cmake/CompilerOptions.cmake

function(target_apply_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /WX /DNOMINMAX /wd4100 /wd4127
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unused-parameter -Wno-unknown-pragmas
        )
    endif()
endfunction()
```

**`function(name arg1 arg2 ...)`** defines a reusable CMake function. Arguments are referenced by name inside the function. Unlike CMake macros, functions have their own variable scope — assignments inside don't leak out.

**`target_compile_options(${target} PRIVATE ...)`** appends compiler flags to the named target. These are flags the compiler receives directly, not abstracted by CMake. They are platform-specific, hence the `if(MSVC)` split.

**Key flags explained:**

| Flag (GCC/Clang) | Meaning |
|---|---|
| `-Wall` | Enable most common warnings |
| `-Wextra` | Enable extra warnings beyond `-Wall` |
| `-Wpedantic` | Warn on non-standard extensions |
| `-Werror` | Treat any warning as an error |
| `-Wno-unused-parameter` | Suppress "unused parameter" (common in virtual function overrides) |
| `-Wno-unknown-pragmas` | Suppress warnings about `#pragma` directives the compiler doesn't recognize |

**Why `-Werror`?** It forces us to write warning-free code. A warning today is a bug tomorrow — treating them as errors prevents accumulation.

**Why a reusable function?** We apply the same flags to three targets: `myownql`, `myownquantlib_demo`, and `myownquantlib_tests`. A function avoids repeating the same block three times and ensures they stay in sync.

---

## 17. `message()` — the build summary

```cmake
message(STATUS "")
message(STATUS "  myownquantlib ${PROJECT_VERSION}")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ std    : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
```

`message(STATUS "...")` prints to stdout during the configure step (prefixed with `--`). It is CMake's equivalent of `std::cout`.

Other message modes: `WARNING`, `AUTHOR_WARNING`, `SEND_ERROR` (configure continues), `FATAL_ERROR` (configure stops).

The summary is purely for developer convenience. Running `cmake -B build` produces:

```
--
--   myownquantlib 0.1.0
--   Build type : Debug
--   C++ std    : 17
--   Compiler   : AppleClang 17.0.0.17000604
--
```

At a glance you can confirm which compiler and flags are in use.

---

## 18. vcpkg.json — the dependency manifest

```json
{
    "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg/master/scripts/vcpkg.schema.json",
    "name": "myownquantlib",
    "version": "0.1.0",
    "description": "Rebuilding QuantLib from scratch",
    "dependencies": [
        "boost-test"
    ]
}
```

This file uses *manifest mode* — vcpkg's modern, per-project dependency declaration format.

**`"dependencies"`** lists the vcpkg port names. When CMake's configure step runs with the vcpkg toolchain file active, vcpkg reads this file and installs any missing packages into `vcpkg_installed/` automatically. The installed packages are local to this project — they do not touch the global system.

**Why only `boost-test`?** We are rebuilding the math layer from scratch (Phase 3 in `PLAN.md`), so we deliberately avoid Boost.Math, Eigen, or any other math library. The only external dependency is the test framework. Every other component will be implemented by hand.

**`"$schema"`** enables IDE validation (VSCode flags typos in the JSON against the schema). It has no effect on vcpkg itself.

---

## 19. The full build workflow

Here is the complete sequence from a clean checkout to a passing test:

```bash
# 1. Clone/enter the repo
cd myownquantlib

# 2. Configure — runs vcpkg install + generates build files
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake

# 3. Build — compiles all targets
cmake --build build --parallel

# 4. Run the demo
./build/myownquantlib_demo

# 5. Run all tests via CTest
ctest --test-dir build --output-on-failure

# 5a. Or run a specific test suite directly
./build/myownquantlib_tests --run_test=YieldCurveTests

# 5b. Or run a single test case
./build/myownquantlib_tests --run_test=OptionPricerTests/test_put_call_parity
```

You only need to re-run step 2 when:
- You add a new `.cpp` source file (add it to `CMakeLists.txt`)
- You add a new dependency to `vcpkg.json`
- You change a `CMakeLists.txt`

You re-run step 3 on every code change (it is incremental — only changed files are recompiled).

---

## 20. Directory layout explained

```
myownquantlib/
│
├── CMakeLists.txt          ← Root: project(), global settings, add_subdirectory()
├── vcpkg.json              ← Dependency manifest (only boost-test)
│
├── cmake/
│   └── CompilerOptions.cmake  ← Reusable function: target_apply_compiler_options()
│
├── include/
│   └── myownql/
│       └── version.hpp     ← Public headers (accessible as <myownql/...>)
│
├── src/
│   ├── CMakeLists.txt      ← Defines the myownql STATIC library
│   ├── placeholder.cpp     ← Temporary; removed once Phase 1 adds real sources
│   └── core/              ← (Phase 1 and beyond: errors.hpp, observable.hpp, …)
│
├── tests/
│   ├── CMakeLists.txt      ← Defines the test executable, add_test()
│   └── test_main.cpp       ← Boost.Test entry point + placeholder test
│
└── main.cpp                ← Demo executable source (grows with each phase)
```

**Why separate `include/` from `src/`?**

`include/myownql/` contains *public API* — headers that users of the library include. These are stable, minimal, and intentionally exposed.

`src/` contains *implementation* — `.cpp` files and any private headers that are internal to the library. No consumer should include these directly.

This separation is enforced by CMake: `include/` is `PUBLIC`, `src/` is `PRIVATE` (see section 10). It mirrors how real-world C++ libraries are structured (e.g., Boost, Qt, QuantLib itself).

---

## What comes next

With the scaffold in place, Phase 1 can begin. The workflow for every new file in Phase 1 will be:

1. Create `src/core/errors.hpp` (or `.cpp`)
2. Add the `.cpp` to the sources list in `src/CMakeLists.txt`
3. Create `tests/core/test_errors.cpp` and add it to `tests/CMakeLists.txt`
4. Run `cmake --build build && ctest --test-dir build`

No changes to the build system configuration (`cmake -B build`) are needed — adding sources to an existing target only requires a rebuild, not a reconfigure.
