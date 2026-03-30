# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (vcpkg recommended)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake

# Or against a local QuantLib installation
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/ql-install

# Build
cmake --build build --parallel

# Run demo
./build/myownquantlib_demo

# Run tests
ctest --test-dir build --output-on-failure
# Or directly:
./build/myownquantlib_tests

# Run a single test suite or case (Boost.Test syntax)
./build/myownquantlib_tests --run_test=YieldCurveTests
./build/myownquantlib_tests --run_test=OptionPricerTests/test_put_call_parity
```

## Architecture

Everything lives under the `myownql` namespace. The library wraps QuantLib with two focused modules:

### YieldCurveBuilder (`src/curves/`)
Fluent builder pattern over QuantLib's verbose `PiecewiseYieldCurve` bootstrapping. Internally uses:
- `DepositRateHelper` (Actual/360) for short-end deposits
- `SwapRateHelper` with Euribor 6M (Thirty360) for swaps
- TARGET calendar, T+2 settlement
- `PiecewiseYieldCurve<ZeroYield, Linear>` as the output type (aliased as `BootstrappedCurve`)

```cpp
auto curve = YieldCurveBuilder(today)
    .addDeposit(3*Months, 0.039)
    .addSwap(10*Years, 0.048)
    .build(/*enableExtrapolation=*/true);
```

### OptionPricer (`src/pricing/`)
Vanilla option pricer supporting multiple QuantLib engines selected at runtime via `Engine` enum:
- `Engine::BlackScholes` — analytic BSM (European only)
- `Engine::BinomialCRR` — CRR tree (European + American)
- `Engine::FiniteDiff` — PDE solver (European + American)
- `Engine::BaroneAdesi` — BAW approximation (American only)

Market data is stored as `SimpleQuote` objects; `setSpot()`, `setVol()`, `setRate()`, `setDividend()` update them reactively without rebuilding the process. `price()` returns a `GreekResult` struct (NPV + all first-order Greeks). `impliedVol()` backs out IV from a market price.

### main.cpp
Treated as runnable documentation — it demonstrates curve construction, engine comparison, Greeks, implied vol smile, reactive repricing, and NPV grids. Read this first when learning the API.

## Dependencies & Toolchain
- **C++17**, CMake ≥ 3.15
- **QuantLib** and **Boost.Test** managed via vcpkg (`vcpkg.json`)
- Compiler warnings enforced via `cmake/CompilerOptions.cmake` (`target_apply_compiler_options`); QuantLib/Boost header warnings are suppressed intentionally

## Test Structure
Tests use Boost.Test and are in `tests/test_basic.cpp`, organized into two suites:
- `YieldCurveTests` — curve bootstrapping and discount factor monotonicity
- `OptionPricerTests` — put-call parity, delta bounds, American ≥ European, implied vol roundtrip
