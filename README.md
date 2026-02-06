# gitmem

An experimental interpreter and model checker for exploring concurrent programs with Git-inspired memory semantics. Gitmem allows you to write multi-threaded programs and automatically explore all possible interleavings to detect data races, deadlocks, and assertion failures.

## Overview

Gitmem is a research tool that models concurrent memory operations using version control semantics. It provides:

- **Multiple sync protocols**: Linear and branching (with eager/lazy variants) semantics for thread synchronization
- **Automatic model checking**: Explores all possible execution paths to find concurrency bugs
- **Interactive debugging**: Step through different thread schedules interactively
- **Execution visualization**: Generates GraphViz diagrams showing execution traces and revision graphs

## Language Features

The gitmem language supports:

- **Shared variables**: `x = value` (global shared state)
- **Thread-local registers**: `$r = value` (prefixed with `$`)
- **Thread operations**: `spawn { ... }`, `join $thread`
- **Synchronization**: `lock var`, `unlock var`
- **Control flow**: `if (condition) { ... } else { ... }`
- **Assertions**: `assert(condition)`
- **Operators**: `==`, `!=`, `+`

### Example Program

```gitmem
x = 0;
$t1 = spawn {
    lock l;
    x = x + 1;
    unlock l;
};
$t2 = spawn {
    lock l;
    x = x + 1;
    unlock l;
};
join $t1;
join $t2;
assert(x == 2);
```

## Building and Running

### Prerequisites

- CMake (3.14+)
- Ninja build system
- C++23 compatible compiler (Clang recommended)
- Python 3 (for tests)

### Build Instructions

```bash
mkdir build
cd build
cmake -G Ninja .. -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
ninja
```

Optional CMake flags:
- `-DCMAKE_CXX_STANDARD=23` - Set C++ standard (if needed)
- `-DCMAKE_BUILD_TYPE=Release` - Build optimized version

### Quick Test

Test your build with:

```bash
./gitmem -e ../examples/race_condition.gm
```

## Usage

### Basic Execution

Run a single execution trace:

```bash
./gitmem examples/race_condition.gm
```

This generates a GraphViz `.dot` file showing the execution trace.

### Model Checking Mode

Explore all possible execution paths:

```bash
./gitmem -e examples/race_condition.gm
```

Model checking will:
- Try all possible thread interleavings
- Report data races, deadlocks, and assertion failures
- Generate execution diagrams for failing traces
- Exit with status 0 if all paths succeed, non-zero if any fail

### Interactive Mode

Step through executions manually:

```bash
./gitmem -i examples/singleton.gm
```

Commands in interactive mode:
- `?` - Show help
- `s` - Show current state
- `n` - Step one thread forward
- `r` - Run to next synchronization point

### Sync Protocols

Gitmem supports different memory models:

#### Linear (Default)
```bash
./gitmem --sync linear program.gm
```
Traditional sequential consistency model.

#### Branching (Eager)
```bash
./gitmem --sync branching --branching-mode eager program.gm
```
Git-like branching semantics where threads create branches that merge at synchronization points. Conflicts are detected eagerly.

#### Branching (Lazy)
```bash
./gitmem --sync branching --branching-mode lazy program.gm
```
Lazy conflict detection variant that defers checking until synchronization.

Additional flags:
- `--include-empty-commits` - Include empty commits in branching output
- `--raise-early-conflicts` - Raise conflicts before write suppression (lazy mode only)
- `-v, --verbose` - Enable verbose interpreter output
- `-o, --output <file>` - Specify output file path

## Testing

Run the test suite:

```bash
# From build directory
ninja run_gitmem_tests

# Or using CTest
ctest
```

The test suite includes:
- **Accept tests**: Programs that should execute successfully
- **Reject tests**: Programs with errors (deadlocks, races, assertion failures)
- Tests for both linear and branching semantics

## Project Structure

```
src/
  ├── gitmem.cc              - Main entry point
  ├── lang.hh                - Language token definitions
  ├── parser.cc              - Parser implementation
  ├── interpreter.cc         - Interpreter core
  ├── model_checker.cc       - Model checking engine
  ├── debugger.cc            - Interactive debugger
  ├── execution_state.hh     - Thread and memory state
  ├── sync_protocol.hh       - Sync protocol interface
  ├── linear/                - Linear sync protocol
  └── branching/             - Branching sync protocols
      ├── base_sync_protocol.cc
      ├── eager/             - Eager conflict detection
      └── lazy/              - Lazy conflict detection

examples/
  ├── accept/semantics/      - Valid programs
  │   ├── linear/            - Linear semantics tests
  │   └── branching/         - Branching semantics tests
  └── reject/semantics/      - Programs with bugs
      ├── linear/            - Deadlocks, races for linear
      └── branching/         - Bugs for branching semantics
```

## Executables

The build produces two binaries:

### `gitmem`
The main interpreter and model checker. Executes programs and generates execution diagrams.

### `gitmem_trieste`
Parser diagnostic tool built on [Trieste](https://github.com/microsoft/Trieste). Use it to inspect the AST:

```bash
./gitmem_trieste build program.gm
# Creates program.trieste with S-expression AST
```

## VSCode Extension

A syntax highlighting extension is available in `gitmem-extension/`.

Install via:
1. Open VSCode Command Palette (`Cmd+Shift+P` / `Ctrl+Shift+P`)
2. Run: `Developer: Install Extension from Location`
3. Select the `gitmem-extension` directory

This provides syntax highlighting for `.gm` files.

## Output

Gitmem generates GraphViz `.dot` files visualizing:

- **Execution traces**: Thread operations and memory states
- **Revision graphs**: For branching semantics, shows branch/merge structure
- **Conflict detection**: Highlights data races and conflicts

View `.dot` files with GraphViz:

```bash
dot -Tpng output.dot -o output.png
```

## Exit Codes

- `0` - All execution paths succeeded
- `1` - Assertion failure, deadlock, data race, or error detected
- Other non-zero codes indicate internal errors