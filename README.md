# myownquantlib

A personal quantitative finance library built on top of [QuantLib](https://www.quantlib.org/).

## Project Structure

```
myownquantlib/
├── CMakeLists.txt                 Root build file
├── vcpkg.json                     Dependency manifest (QuantLib + Boost)
├── cmake/
│   └── CompilerOptions.cmake      Reusable compiler flag helper
├── include/
│   └── myownquantlib/
│       └── version.hpp
├── src/
│   ├── CMakeLists.txt
│   ├── curves/
│   │   ├── YieldCurveBuilder.hpp  Fluent builder for bootstrapped yield curves
│   │   └── YieldCurveBuilder.cpp
│   └── pricing/
│       ├── OptionPricer.hpp       Self-contained vanilla option pricer
│       └── OptionPricer.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_basic.cpp             Boost.Test suite
└── main.cpp                       End-to-end demo
```

## Tech Stack

| Component | Choice | Reason |
|-----------|--------|--------|
| Language  | C++17 | Matches QuantLib's own requirement |
| Compiler  | Apple Clang 17 (arm64) | System compiler on this machine |
| Build     | CMake ≥ 3.15 | Same as QuantLib |
| Dependencies | vcpkg (manifest mode) | Reproducible, per-project dependency isolation |
| QuantLib  | via vcpkg `quantlib` port | Pulls QuantLib + Boost automatically |
| Test framework | Boost.Test | Same framework as QuantLib's own test-suite |

## Prerequisites

### 1. Install vcpkg

vcpkg is **not** installed on this machine yet. Run once:

```bash
cd ~
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
```

### 2. Build

```bash
cd /Users/hanjingong/GolandProjects/myownquantlib

# Configure — vcpkg toolchain makes find_package(QuantLib) work automatically
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake

# vcpkg will download and compile QuantLib + Boost the first time (~10–20 min)
cmake --build build --parallel

# Run the demo
./build/myownquantlib_demo

# Run tests
ctest --test-dir build --output-on-failure
```

> **Tip:** vcpkg stores built packages under `vcpkg_installed/` in this directory.
> Subsequent builds are fast because packages are cached.

## Alternatively: Use the local QuantLib source

If you've already built QuantLib from source in `../QuantLib/build`, you can skip vcpkg
and point CMake at the local installation:

```bash
# First, install QuantLib to a local prefix
cmake -B ../QuantLib/build ../QuantLib \
  -DCMAKE_INSTALL_PREFIX=~/ql-install
cmake --build ../QuantLib/build --parallel
cmake --install ../QuantLib/build

# Then configure myownquantlib against it
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=~/ql-install
cmake --build build --parallel
```

## Dependency Notes

QuantLib's only mandatory dependency is **Boost ≥ 1.58**.
vcpkg's `quantlib` port declares this automatically — you don't need to install Boost separately.

Key Boost components pulled in by QuantLib:
- `boost::math` (special functions, distributions)
- `boost::accumulators` (statistics)
- `boost::numeric::ublas` (matrix operations)
- `boost::date_time` (date arithmetic)
- `boost::signals2` (thread-safe observer, optional)
- `boost::test` (used here for the test suite)

Modern defaults (`std::any`, `std::optional`) reduce Boost coupling further.

## Related

- `../QuantLib/` — QuantLib library source (for reading)
- `../QuantLib/sandbox/` — Hands-on experiments with the upstream library
- `../QuantLib/study-notes/` — Structured theory notes
