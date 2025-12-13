# CIMA LLVM
An implementation of [CIMA: Compiler-Enforced Resilience Against Memory Safety Attacks in Cyber-Physical Systems](https://www.sciencedirect.com/science/article/pii/S0167404820301061)

## Dynamic Taint Tracking
This extension to CIMA uses Dynamic Taint Tracking to propagate tainted undefined values from CIMA, guarding memory stores from tainted data.

## Repository Organization

- `cimapass/` - LLVM optimization passes implementing CIMA variants
  - `CIMAPass.so` - Base pass with graceful degradation
  - `CIMAPassNearestValid.so` - Nearest-valid memory recovery
  - `CIMAPassTainted.so` - Dynamic taint tracking
  - `cima_runtime.cpp` - Runtime support for nearest-valid search

- `tests/` - Test suite with execution pipeline
  - `basic_tests/` - Memory safety tests (OOB, UAF, buffer overflow)
  - `taint_tests/` - Taint propagation and control flow tests
  - `nearest_valid_tests/` - Nearest-valid recovery tests
  - `pipeline_unified.sh` - Test execution script
  - `benchmark_dir.py` - Performance benchmarking tool

- `stats/` - Benchmark results and performance data

## Build

Requirements:
- LLVM 3.20+
- CMake
- Clang

Build the passes:
```bash
./build.sh
```

This compiles the LLVM passes to `build/cimapass/`.

## Usage

Run tests with the unified pipeline:
```bash
./tests/pipeline_unified.sh <source.c> --pass=VARIANT [OPTIONS]
```

Pass variants:
- `base` - Basic CIMA with undefined value recovery
- `nearest` - Nearest-valid memory recovery
- `tainted` - Dynamic taint tracking
- `all` - Run all variants
- 'asan' - Compiles with ASan only
- `none` - Compile without CIMA or ASan (baseline)

Options:
- `--cfg` - Generate control flow graph PDFs
- `--debug` - Enable debug output (tainted pass)
- `--nearest-valid` - Enable nearest-valid flag
- `--keep-ir` - Preserve intermediate LLVM IR files

Example:
```bash
./tests/pipeline_unified.sh tests/basic_tests/oob.c --pass=all --cfg
```

## Testing

The test suite includes:
- **Basic tests** - Memory safety violations (9 tests)
- **Taint tests** - Dynamic taint tracking scenarios (13 tests)
- **Nearest-valid tests** - Memory recovery (2 tests)

Run benchmarks:
```bash
python3 tests/benchmark_dir.py tests/build_tests/
```
