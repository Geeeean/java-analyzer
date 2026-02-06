# High-Performance Hybrid Analysis Framework

A high-performance hybrid analyzer combining unbounded static analysis with coverage-guided fuzzing for JVM bytecode analysis. This framework implements deterministic parallel fixpoint computation using Weak Partial Ordering (WPO) to achieve significant speedup on complex control-flow programs.

## Overview

This project investigates the limits of parallelization in hybrid program analysis while maintaining correctness. The system couples:

- **Unbounded Static Analyzer**: Uses interval abstraction with WPO-based parallel fixpoint computation
- **Coverage-Guided Fuzzer**: Parallel fuzzing pool guided by static analysis results to identify high-interest targets and prune dead input spaces

The framework is implemented in C for maximum performance and low-level control over parallelism.

## Platform Support

**Supported**: POSIX-compliant systems (Linux, macOS)  
**Not Supported**: Windows

The project has been developed and tested exclusively on POSIX systems. Windows compatibility is not guaranteed.

## Prerequisites

- **GCC 14+** (https://gcc.gnu.org/gcc-14/)
  - Other compilers may work but are untested
  - To use a different compiler, modify the `CC` variable in the Makefile

## Installation

### 1. Install Dependencies

From the project root directory:

```sh
chmod +x install_deps.sh
./install_deps.sh
```

This script downloads, compiles, and installs the required external libraries into the `lib/` directory.

### 2. Build the Project

```sh
make
```

Upon successful compilation, binaries will be located in the `bin/` directory.

## Configuration

Create a configuration file at `$HOME/.config/java-analyzer/java-analyzer.conf`:

```ini
name                  hybrid_analyzer
version               1.0
group                 dtu_compute_group29
for_science           0
tags                  static,dynamic,parallel
jpamb_source_path     /path/to/jpamb/src/main/java
jpamb_decompiled_path /path/to/jpamb/decompiled
```

**Important**: Update the `jpamb_source_path` and `jpamb_decompiled_path` to point to your local JPAMB benchmark suite installation.

### JPAMB Benchmark Suite

The framework is evaluated using the JPAMB benchmark suite:  
https://github.com/kalhauge/jpamb

Clone and build JPAMB before running analysis tasks.

## Usage

### Basic Execution

```sh
./bin/analyzer
```

### Analyze Specific Test Cases

```sh
# Example: Divide by zero test
./bin/analyzer "jpamb/cases/Simple.divideByZero:()I"

# Example: Array spell checking
./bin/analyzer -f "jpamb/cases/Arrays.arraySpellsHello:([C)V"
```

### Debug Build with Memory Checking

For development and debugging:

```sh
# Clean and rebuild with debug symbols
make clean && make CFLAGS="-g -O0"

# Run with Valgrind memory checking
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --error-exitcode=1 \
         ./bin/analyzer -f "jpamb/cases/Arrays.arraySpellsHello:([C)V"
```

## Performance Characteristics

The parallel implementation exhibits three distinct performance patterns:

1. **Performance Degradation**: Small CFGs or extremely low widening thresholds (synchronization overhead dominates)
2. **Weak Speedup**: Simple loops with low widening thresholds (≤50)
3. **Strong Speedup**: Complex loops with high widening thresholds (≥500)

Optimal parallelization is achieved on programs with:
- Complex cyclic control flow structures
- High widening thresholds (500+)
- Multiple independent loops that can be analyzed concurrently

For small inputs where sequential analysis completes in microseconds, the overhead of parallelization may exceed the benefits.

## Architecture

### Static Analysis Phase
- Builds context-sensitive CFG with inlined function calls
- Applies WPO decomposition to identify parallelizable components
- Computes abstract fixpoint using interval analysis
- Generates guidance map identifying:
  - **High-Interest Targets**: Reachable blocks behind complex checks
  - **Dead Input Spaces**: Statically unreachable paths

### Dynamic Analysis Phase
- Spawns N parallel fuzzer threads (configurable)
- Shared-memory architecture with atomic coverage bitmap updates
- Fuzzers consume static guidance to:
  - Skip inputs leading to dead paths
  - Prioritize inputs reaching high-interest targets

## Project Structure

```
.
├── bin/              # Compiled binaries
├── lib/              # External dependencies
├── src/              # Source code
├── Makefile          # Build configuration
└── install_deps.sh   # Dependency installation script
```

## References

This implementation is based on research in hybrid program analysis and parallel fixpoint computation:

- **Deterministic Parallel Fixpoint Computation**  
  Kim, Venet & Thakur (2019)  
  https://arxiv.org/abs/1909.05951

- **Parallel Fuzzing Techniques**  
  Wang et al. (2021)  
  https://www.sciencedirect.com/science/article/pii/S0164121221001394

- **Abstract Interpretation Foundations**  
  https://arxiv.org/abs/2009.05865

## License

This project was developed as part of coursework at DTU Compute, Technical University of Denmark.

## Troubleshooting

### Build Failures

If compilation fails:
1. Verify GCC version: `gcc --version`
2. Ensure dependencies installed successfully: `ls -l lib/`
3. Check for error messages in build output

### Configuration Issues

If the analyzer can't find test cases:
1. Verify `java-analyzer.conf` exists in `$HOME/.config/java-analyzer/`
2. Confirm JPAMB paths are absolute and point to valid directories
3. Ensure JPAMB is built and decompiled classes are present

### Runtime Errors

For segmentation faults or crashes:
1. Rebuild with debug symbols: `make clean && make CFLAGS="-g -O0"`
2. Run under GDB: `gdb --args ./bin/analyzer [arguments]`
3. Check for memory leaks with Valgrind (see debug build example above)

## Future Work

Planned enhancements include:
- Support for richer abstract domains beyond interval analysis
- Distance-based guidance metrics for fuzzer
- Tighter feedback loops between static and dynamic phases
- Production-ready error handling and logging
