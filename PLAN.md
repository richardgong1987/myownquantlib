# Rebuilding QuantLib from Scratch — Learning Plan

This document is the master plan for incrementally rebuilding the core of `QuantLib/ql` inside this repository. Each phase has clear **C++ concepts**, **finance concepts**, deliverable files, and validation criteria. Phases are ordered by dependency: nothing in a later phase is needed to implement an earlier one.

---

## Guiding Principles

- Mirror QuantLib's module layout under `src/` and `include/myownql/`
- Build only what we understand — no copy-pasting; every line should be explainable
- Each phase ends with a working test suite and a demo snippet in `main.cpp`
- Use vcpkg + CMake throughout; no external math libraries (we build our own)
- Namespace: `myownql` throughout

---

## Dependency Layers (overview)

```
Phase 1  ── Core infrastructure (errors, types, settings, observer, handle, lazy)
Phase 2  ── Time (Date, Calendar, DayCounter, Period, Schedule)
Phase 3  ── Math (Array, Matrix, interpolation, root-finding, integration, distributions)
Phase 4  ── Market data (Quote hierarchy, Index base)
Phase 5  ── Term structures (Yield curves, bootstrapping)
Phase 6  ── Cash flows & coupons
Phase 7  ── Stochastic processes
Phase 8  ── Numerical methods (lattices, finite differences, Monte Carlo)
Phase 9  ── Instruments & pricing engines (vanilla options, bonds, swaps)
Phase 10 ── Models (short-rate, Heston) & calibration
```

---

## Phase 1 — Core Infrastructure

### Goals
**C++:** namespaces, type aliases, macros vs. `constexpr`, exception hierarchies, CRTP, templates, `shared_ptr` / `weak_ptr`, static assertions.
**Finance:** why a global evaluation date matters; what "observable" market data means.

### Files to create
```
src/core/errors.hpp          – QL_REQUIRE / QL_FAIL macros, Error base class
src/core/types.hpp           – Real, Integer, Natural, Size, Rate, Spread, …
src/core/settings.hpp        – Singleton holding evaluationDate (global clock)
src/core/patterns/
    observable.hpp           – Observable / Observer base classes
    lazyobject.hpp           – LazyObject (caches calculation, auto-invalidates)
    singleton.hpp            – Singleton<T> CRTP template
src/core/handle.hpp          – Handle<T> (relinkable shared_ptr + observer)
```

### Key design decisions to understand
- `Observable` holds a list of `weak_ptr<Observer>` so it does not own its observers
- `LazyObject` is both an `Observer` (it watches its inputs) and an `Observable` (instruments watch it)
- `Handle<T>` wraps a `shared_ptr<Link>` where `Link` holds the actual `shared_ptr<T>`; relinking notifies all observers without replacing the Handle itself
- `Settings` stores the evaluation date as a `Date`; changing it notifies all dependent lazy objects

### Validation
- Unit tests: observer notification counts, lazy recalculation triggers, handle relinking
- Demo: create two `SimpleQuote`s, observe them from a custom observer, mutate a quote, verify notification fires

---

## Phase 2 — Time

### Goals
**C++:** operator overloading, `enum class`, `std::chrono` vs. custom date arithmetic, virtual dispatch, RTTI alternatives.
**Finance:** business day conventions, day count fractions, settlement lag, coupon schedule generation.

### Files to create
```
src/time/date.hpp            – Date (serial-number based, year/month/day API)
src/time/period.hpp          – Period (n × TimeUnit), frequency enum
src/time/timeunit.hpp        – TimeUnit enum (Days, Weeks, Months, Years)
src/time/calendar.hpp        – Calendar base class (isBusinessDay, advance, businessDaysBetween)
src/time/calendars/
    target.hpp               – TARGET (EU) calendar
    unitedstates.hpp         – US calendars (Settlement, NYSE, …)
    unitedkingdom.hpp        – UK calendars
src/time/daycounter.hpp      – DayCounter base (dayCount, yearFraction)
src/time/daycounters/
    actual360.hpp
    actual365fixed.hpp
    actualactual.hpp         – ISMA, ISDA, Bond variants
    thirty360.hpp            – BondBasis, EurobondBasis
src/time/schedule.hpp        – Schedule (vector of dates from rule + calendar)
```

### Key design decisions to understand
- `Date` stores a serial integer (days since an epoch); all arithmetic is integer arithmetic
- `Calendar` stores a set of holidays; subclasses override `addHolidays()` which runs once
- `DayCounter` is stateless — it holds no data, just a vtable. Use value semantics with `impl` pointer (pimpl)
- `Schedule` is generated once and stored as `std::vector<Date>`; it is not lazy

### Validation
- Test: TARGET calendar advances over Easter, year-end; US calendar skips Thanksgiving
- Test: Actual/365 and 30/360 yearFractions against known textbook values
- Test: Schedule with monthly frequency, end-of-month rule produces correct stub

---

## Phase 3 — Math

### Goals
**C++:** expression templates (CRTP for `Array`), template metaprogramming, function objects vs. lambdas, iterator protocol, LAPACK alternatives.
**Finance:** interpolation of yield curves; numerical integration for option pricing; normal distribution for Black-Scholes.

### Files to create
```
src/math/array.hpp               – 1-D array with arithmetic operators
src/math/matrix.hpp              – 2-D matrix, transpose, multiply
src/math/comparison.hpp          – close_enough, IS_EQUAL, tolerances

src/math/interpolations/
    interpolation.hpp            – Interpolation base (operator(), derivative, primitive)
    linearinterpolation.hpp
    loglinearinterpolation.hpp
    cubicinterpolation.hpp       – natural/clamped cubic spline
    backwardflatinterpolation.hpp

src/math/solvers1d/
    solver1d.hpp                 – Solver1D base (solve, bracket)
    brentsolve.hpp               – Brent's method
    newtonraphson.hpp

src/math/integrals/
    integral.hpp                 – Integral base
    simpsonintegral.hpp
    gausslegendreintegration.hpp

src/math/distributions/
    normaldistribution.hpp       – CumulativeNormalDistribution, InverseCumNormal
    poissondistribution.hpp

src/math/randomnumbers/
    mt19937uniformrng.hpp        – Mersenne Twister wrapper
    sobolrsg.hpp                 – Sobol low-discrepancy sequence

src/math/optimization/
    problem.hpp                  – CostFunction + Constraint
    levenbergmarquardt.hpp
    simplex.hpp
```

### Key design decisions to understand
- `Array` owns its data via `std::vector<Real>`; arithmetic ops return new `Array` (value semantics)
- `Interpolation` objects are constructed from a pair of iterators `[xBegin, xEnd)` and matching y values; they pre-compute coefficients in the constructor
- Solvers use a bracket `[a,b]` + tolerance + max-iterations pattern; `Brent` is the default
- `CumulativeNormalDistribution` implements the Hart approximation (no `<cmath>` erf)

### Validation
- Test: linear interp matches exact values at nodes, interpolates correctly between
- Test: Brent solver finds root of `x^2 - 2` to 1e-12
- Test: Gaussian Legendre integrates `exp(-x^2)` accurately
- Test: CumulativeNormal at 0 = 0.5, at 1.645 ≈ 0.95

---

## Phase 4 — Market Data (Quotes & Indexes)

### Goals
**C++:** abstract interfaces, dynamic_cast, `std::function`, template type deduction.
**Finance:** what a "quote" means (a live, changeable market observable); how IBOR fixings work; fixing calendar vs. value calendar.

### Files to create
```
src/quotes/
    quote.hpp                – Quote abstract base (value(), isValid(), Observable)
    simplequote.hpp          – SimpleQuote (setValue, holds a Real)
    derivedquote.hpp         – DerivedQuote<UnaryFunction>
    compositequote.hpp       – CompositeQuote<BinaryFunction>(q1, q2)

src/indexes/
    index.hpp                – Index base (name, fixing, addFixing, clearFixings)
    interestrateindex.hpp    – InterestRateIndex (currency, tenor, settlement lag, calendar, daycounter)
    iborindex.hpp            – IborIndex (maturityDate, forecastFixing from yield curve)
    ibor/
        euribor.hpp          – Euribor 1W/1M/3M/6M/1Y
        usdlibor.hpp         – USD LIBOR
        sonia.hpp
        sofr.hpp
```

### Key design decisions to understand
- `Quote` is a pure interface; it inherits `Observable` so anything watching a quote gets notified on change
- `SimpleQuote::setValue` calls `notifyObservers()` — this is the entry point of the reactive chain
- `IborIndex` stores a `Handle<YieldTermStructure>` for forecasting; changing the handle notifies the index, which notifies all instruments using it
- Fixing storage is a static `std::map<std::string, TimeSeries<Real>>` inside `IndexManager` (a `Singleton`)

### Validation
- Test: SimpleQuote notification count
- Test: DerivedQuote reflects source changes
- Test: IborIndex returns stored historical fixing; throws when fixing missing and no forecast curve attached

---

## Phase 5 — Term Structures & Yield Curves

### Goals
**C++:** CRTP for mixins (e.g., `InterpolatedCurve<Interpolator>`), policy-based design, virtual inheritance, template template parameters.
**Finance:** zero rates vs. discount factors vs. forward rates; bootstrapping; interpolation conventions on yield curves.

### Files to create
```
src/termstructures/
    termstructure.hpp               – TermStructure base (referenceDate, calendar, dayCounter, maxDate)
    yieldtermstructure.hpp          – YieldTermStructure (discount, zeroRate, forwardRate — all virtual)

    yield/
        flatforward.hpp             – FlatForward (constant rate curve)
        zerospreadedtermstructure.hpp
        discountcurve.hpp           – InterpolatedDiscountCurve<Interpolator>
        zerocurve.hpp               – InterpolatedZeroCurve<Interpolator>
        forwardcurve.hpp            – InterpolatedForwardCurve<Interpolator>

        bootstraphelper.hpp         – BootstrapHelper<TS> base (quote, pillar date, impliedQuote)
        ratehelpers/
            depositratehelper.hpp   – Deposit (O/N to 1Y)
            frahelper.hpp           – FRA
            swapratehelper.hpp      – Vanilla interest rate swap
            bondhelper.hpp          – Bond yield helper

        piecewiseyieldcurve.hpp     – PiecewiseYieldCurve<Traits, Interpolator, Bootstrap>
        iterativebootstrap.hpp      – IterativeBootstrap (default) – root-find each pillar sequentially
```

### Key design decisions to understand
- `YieldTermStructure` provides three equivalent representations (discount, zero, forward) and each concrete subclass overrides only the one that is natural; the other two are implemented via:
  - `discount(t) = exp(-zeroRate(t) * t)`
  - `forwardRate(t1,t2) = -log(discount(t2)/discount(t1)) / (t2-t1)`
- `PiecewiseYieldCurve` stores a sorted vector of `(pillar_date, rate)` pairs; the bootstrap fills these sequentially using Brent's solver so that each helper's implied quote matches its market quote
- `BootstrapHelper` is both `LazyObject` (it caches its implied rate) and `Observer` of its input `Quote`

### Validation
- Test: FlatForward — discount(1Y) = exp(-r) for continuous compounding
- Test: DiscountCurve — interpolated discount factors, forward rates are positive
- Test: Bootstrap 3-instrument curve (1M deposit, 6M FRA, 5Y swap) reprices all three to par
- Test: Curve notifies observers when linked quote changes

---

## Phase 6 — Cash Flows & Coupons

### Goals
**C++:** virtual destructors, `std::accumulate`, visitor pattern, `std::variant` (C++17).
**Finance:** NPV vs. BPS; duration and convexity; fixed vs. floating coupon mechanics.

### Files to create
```
src/cashflows/
    cashflow.hpp                – CashFlow base (date(), amount(), hasOccurred())
    coupon.hpp                  – Coupon base (nominal, accrualStart/End, rate(), DayCounter)
    simplecashflow.hpp          – SimpleCashFlow (redemption, fee)

    fixedratecoupon.hpp         – FixedRateCoupon
    floatingratecoupon.hpp      – FloatingRateCoupon (index, spread, gearing)
    iborcoupon.hpp              – IborCoupon (IBOR fixing + spread)
    overnightindexedcoupon.hpp  – OIS compounding coupon

    cashflows.hpp               – Utility functions:
                                    npv(legs, discount_curve, settlement)
                                    bps(legs, discount_curve, settlement)
                                    duration(legs, yield, dc, compounding, freq)
                                    convexity(...)
                                    yieldToNPV / npvToYield (root-find)

    couponpricer.hpp            – CouponPricer base (initialize, swapletPrice, capletPrice)
    blackiborpricer.hpp         – Prices IBOR coupons with Black vol surface

    cashflowvectors.hpp         – FixedRateLeg, IborLeg, OvernightLeg builders
```

### Key design decisions to understand
- `CashFlow` inherits `Observable`; repricings are lazy — `amount()` triggers recalculation only if dirty
- `IborCoupon::amount()` checks if fixing date is past (use historical fixing) or future (use forecast from yield curve)
- `CashFlows::npv` iterates the leg, calls `discount(cf.date())` on the curve, sums `cf.amount() * df`
- `CouponPricer` is attached to a coupon via `pricer->initialize(coupon)` before calling `swapletPrice()`

### Validation
- Test: FixedRateCoupon amount = nominal × rate × yearFraction
- Test: IborCoupon with historical fixing reproduces exact amount
- Test: `CashFlows::npv` on a single cash flow equals `amount * discount`
- Test: Duration of a zero-coupon bond = maturity

---

## Phase 7 — Stochastic Processes

### Goals
**C++:** pure abstract classes, protected helpers, function composition, `std::tuple`.
**Finance:** Itô's lemma; drift and diffusion; risk-neutral measure; discretization schemes.

### Files to create
```
src/processes/
    stochasticprocess.hpp        – StochasticProcess base
                                       x0(), drift(t,x), diffusion(t,x), evolve(t,x,dt,dw)
                                       apply(x, dx) — default: x + dx

    stochasticprocess1d.hpp      – StochasticProcess1D (scalar specialisation)

    discretization.hpp           – Discretization interface (drift, diffusion, covariance)
    eulerdiscretization.hpp      – Euler-Maruyama (drift*dt, diffusion*sqrt(dt)*dw)

    blackscholesprocess.hpp      – GeneralizedBlackScholesProcess
                                       (x0, dividendTS, riskFreeTS, blackVolTS)
    blackscholesmerton.hpp       – BlackScholesMertonProcess (adds dividend yield)
    hestonprocess.hpp            – HestonProcess (v0, kappa, theta, sigma, rho)
    coxingersollrossprocess.hpp  – CIR short-rate process
    ornsteinuhlenbeckprocess.hpp – OU process (for Hull-White)

    localvoltermstructure.hpp    – LocalVolTermStructure (localVol surface)
    localconstantvol.hpp
    localvolsurface.hpp          – derived from Dupire formula
```

### Key design decisions to understand
- `StochasticProcess` has no state beyond the starting value `x0`; market data is injected via `Handle<TermStructure>`
- `evolve(t, x, dt, dw)` = `apply(x, discretization->drift(...)*dt + discretization->diffusion(...)*dw)` — the discretization is a policy
- `GeneralizedBlackScholesProcess` holds three term structures (dividend, risk-free, vol) via `Handle`; changing any one triggers re-pricing everywhere

### Validation
- Test: OU process mean-reverts to long-term mean in expectation over many paths
- Test: GBM process: `E[S_T] = S_0 * exp((r-q)*T)` via MC
- Test: Heston process: variance process stays positive (Feller condition)

---

## Phase 8 — Numerical Methods

### Goals
**C++:** policy-based design, `std::function` for payoffs, range-v3 / views, template recursion.
**Finance:** risk-neutral pricing via trees; PDE discretization; Monte Carlo with variance reduction.

### 8a — Lattice (Binomial / Trinomial Trees)

```
src/methods/lattices/
    lattice.hpp                 – Lattice base (grid, stepback)
    tree.hpp                    – Tree<T> (T = 1D process discretization)
    binomialtree.hpp            – BinomialTree (u, d, p from process)
    crrtree.hpp                 – Cox-Ross-Rubinstein
    lrtree.hpp                  – Leisen-Reimer
    trinomialtree.hpp
    blackscholeslattice.hpp     – wraps BSM process onto tree
```

### 8b — Finite Differences

```
src/methods/finitedifferences/
    meshers/
        fdmmesher.hpp           – FdmMesher base (locations, dplus, dminus)
        uniform1dmesher.hpp
        concentrating1dmesher.hpp   – denser near barrier/strike
        fdm2dmesher.hpp         – tensor product of two 1-D meshers
    operators/
        fdmlinearopiterator.hpp
        fdmlinearoplayout.hpp
        secondderivativeop.hpp  – d²/dx²
        firstderivativeop.hpp   – d/dx
        triplebandlinearop.hpp  – tridiagonal matrix
    schemes/
        douglasscheme.hpp       – Crank-Nicolson / Douglas
        hundsdorferscheme.hpp   – ADI for 2-D
        cranknicolsonscheme.hpp
    solvers/
        fdm1dblacksckholessolver.hpp
        fdm2dblackscholessolver.hpp
    stepconditions/
        fdmstepcondition.hpp    – American exercise, barrier knock-out
        fdmamericanstepcondition.hpp
```

### 8c — Monte Carlo

```
src/methods/montecarlo/
    sample.hpp                  – Sample<T> (value + weight)
    path.hpp                    – Path (time grid + asset values)
    pathgenerator.hpp           – PathGenerator<RNG> (generates Path)
    multipathgenerator.hpp      – MultiPath for multi-asset
    montecarlomodel.hpp         – MonteCarloModel<MC, RNG, S>
    mcsimulation.hpp            – McSimulation base (addSamples, stats)
    longstaffschwartz.hpp       – Longstaff-Schwartz regression for American options
    earlyexercisepathpricer.hpp
    brownianbridge.hpp          – Brownian bridge for path construction
```

### Key design decisions to understand
- Binomial tree stores `N` arrays (one per time step); stepback sweeps backwards applying payoff at expiry then `max(hold, exercise)` at each node
- FD operators are represented as tridiagonal matrices; applying the operator is a matrix-vector multiply
- ADI (Alternating Direction Implicit) splits the 2-D operator into two 1-D solves to keep it tractable
- MC `PathGenerator` takes a `StochasticProcess` and an `RNG`; it calls `process.evolve()` at each time step
- Longstaff-Schwartz runs two passes: forward (generate paths) + backward (regress continuation value)

### Validation
- Test: CRR tree converges to BS formula as N → ∞ (European call)
- Test: FD theta scheme prices barrier option within 1% of analytic
- Test: MC GBM engine price matches analytic BS for European call (within ~0.5% at N=100k)
- Test: Longstaff-Schwartz prices American put ≥ European put of same strike

---

## Phase 9 — Instruments & Pricing Engines

### Goals
**C++:** non-virtual interface (NVI) idiom, `std::any`, mixin inheritance, tag dispatch.
**Finance:** put-call parity; bond pricing; swap valuation; Greeks via finite differences and analytically.

### 9a — Instrument base & option types

```
src/instrument.hpp              – Instrument base (setPricingEngine, NPV, errorEstimate, isExpired)
src/pricingengine.hpp           – PricingEngine, GenericEngine<Args,Results>

src/instruments/
    payoff.hpp                  – Payoff base, StrikedTypePayoff
    vanillaoption.hpp           – VanillaOption (payoff + exercise)
    exercise.hpp                – Exercise, EuropeanExercise, AmericanExercise, BermudanExercise
    europeanoption.hpp
    americanoption.hpp

src/pricingengines/vanilla/
    analyticeuropeanengine.hpp  – BSM analytic (Black formula)
    binomialvanillaengine.hpp   – Template: BinomialVanillaEngine<Tree>
    fdvanillaengine.hpp         – Base for FD vanilla engines
    fdeuropeanengine.hpp
    fdamericanengine.hpp
    mceuropeanengine.hpp        – MC European engine
    mcamericanengine.hpp        – Longstaff-Schwartz

    blackcalculator.hpp         – Core: price, delta, gamma, vega, theta, rho from d1/d2
```

### 9b — Bonds

```
src/instruments/bonds/
    bond.hpp                    – Bond (cashflows, settlement, yield, DV01)
    fixedratebond.hpp
    floatingratebond.hpp
    zerocouponbond.hpp

src/pricingengines/bond/
    discountingbondengine.hpp   – NPV = sum(cf * discount)
    bondengine.hpp              – Base
```

### 9c — Interest rate swaps

```
src/instruments/
    swap.hpp                    – Swap (two legs, payer/receiver)
    vanillaswap.hpp             – Fixed vs. floating
    overnightindexedswap.hpp    – OIS swap

src/pricingengines/swap/
    discountingswapengine.hpp   – NPV fixed leg - NPV float leg
```

### Key design decisions to understand
- `Instrument::NPV()` calls `calculate()` (inherited from `LazyObject`), which calls the engine's `calculate()`; the engine writes into `results_`, the instrument reads from it
- `GenericEngine<Args,Results>` holds `args_` and `results_`; `calculate()` is pure virtual; `update()` marks instrument dirty via `notifyObservers()`
- `BlackCalculator` is a pure function object; it receives `F, K, sigma, tau, df` and exposes `value(), delta(), gamma(), vega(), theta(), rho()`
- Bond settlement is T+N business days; NPV is always computed on the settlement date, not today

### Validation
- Test: AnalyticEuropeanEngine matches put-call parity exactly
- Test: BinomialVanillaEngine<CRR> with 500 steps within 0.01% of analytic
- Test: FdAmericanEngine American put > European put (same strike, same inputs)
- Test: DiscountingBondEngine: zero-coupon bond price = face × discount(maturity)
- Test: DiscountingSwapEngine: at-market swap NPV ≈ 0

---

## Phase 10 — Models & Calibration

### Goals
**C++:** CRTP calibration mixin, `std::function` cost functions, strategy + template method pattern.
**Finance:** short-rate models; model calibration to swaptions/caps; Heston stochastic vol.

### Files to create

```
src/models/
    model.hpp                   – Model base: params(), calibrate(helpers, method, constraint)
    calibrationhelper.hpp       – CalibrationHelper base (marketValue, modelValue, calibrationError)
    parameter.hpp               – Parameter, ConstantParameter, PiecewiseConstantParameter

    shortrate/
        onefactormodels/
            vasicek.hpp         – dr = a(b-r)dt + σ dW
            hullwhite.hpp       – Hull-White (time-dependent a, σ)
            blackkarasinski.hpp – BK (log-normal short rate)
            coxingersollross.hpp

        twofactormodels/
            g2.hpp              – G2++ (two-factor Gaussian)

        calibrationhelpers/
            swaptionhelper.hpp  – calibrate to swaption vol
            caphelper.hpp       – calibrate to cap vol

    equity/
        hestonmodel.hpp         – Heston: dv = κ(θ-v)dt + σ√v dW_v, corr ρ
        hestonmodelhelper.hpp
        barndorffnielsenshephard.hpp

src/pricingengines/swaption/
    blackswaptionengine.hpp     – analytic Black swaption
    treeswaptionengine.hpp      – lattice (Hull-White tree)
    g2swaptionengine.hpp        – G2++ analytic

src/pricingengines/vanilla/
    analytichestonengine.hpp    – Heston characteristic function + Gauss-Laguerre integration
    mceuropeanhestonengine.hpp
```

### Key design decisions to understand
- `Model::calibrate()` builds a `Problem` (least-squares cost = sum of squared calibration errors), then hands it to an optimizer (`LevenbergMarquardt` by default)
- `CalibrationHelper::calibrationError()` = `(modelValue - marketValue) / marketValue` (relative error)
- Hull-White tree: build trinomial tree on `x = r - φ(t)` where φ is the fit-to-forward correction; pricing on tree is standard stepback
- Heston analytic: price = Re-integration of characteristic function via Gauss-Laguerre quadrature; numerically stable with the "little Heston trap" formulation

### Validation
- Test: Vasicek bond price formula matches closed-form
- Test: Hull-White calibrated to flat vol swaption surface recovers input vols to < 1bp
- Test: AnalyticHestonEngine matches MC Heston engine within MC error
- Test: G2++ two-factor model fits a hump-shaped vol surface better than one-factor HW

---

## Implementation Order Summary

| Phase | Module | Est. files | New C++ concepts |
|-------|--------|-----------|-----------------|
| 1 | Core infrastructure | ~8 | CRTP, weak_ptr, Singleton |
| 2 | Time | ~20 | operator overloading, virtual dispatch |
| 3 | Math | ~25 | expression templates, template policy |
| 4 | Market data | ~12 | abstract interface, static singleton map |
| 5 | Term structures | ~18 | template template params, policy design |
| 6 | Cash flows | ~16 | visitor, std::variant |
| 7 | Stochastic processes | ~12 | pure abstract, function composition |
| 8 | Numerical methods | ~35 | ADI, Longstaff-Schwartz, path gen |
| 9 | Instruments & engines | ~25 | NVI idiom, GenericEngine pattern |
| 10 | Models & calibration | ~20 | least-squares optimization, Fourier |

---

## CMake Structure

```
CMakeLists.txt                    – root; add_subdirectory(src), add_subdirectory(tests)
src/CMakeLists.txt                – builds myownql STATIC library from all .cpp files
tests/CMakeLists.txt              – links against myownql + Boost.Test
cmake/CompilerOptions.cmake       – (already exists)
vcpkg.json                        – add "boost-test" only; math is built from scratch
```

Each phase will add its sources to `src/CMakeLists.txt` and its tests to `tests/`.

---

## What We Are Deliberately NOT Rebuilding

- `experimental/` — too speculative
- `legacy/` — backward-compat shims with no learning value
- Inflation term structures — niche; can be added after Phase 5 if desired
- 90+ calendar implementations — implement TARGET, US, UK as reference; others follow the same pattern
- Exotic instruments (Asian, barrier, etc.) — add after Phase 9 if desired; same engine pattern
- Market models (BGM/LMM) — Phase 10 is already ambitious; LMM is a separate deep topic
