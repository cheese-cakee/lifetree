# LifeTree

Deterministic, memory-safe Directed Acyclic Graph (DAG) for C++ runtime module orchestration.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)]()
[![Tests](https://img.shields.io/badge/Tests-Passing-brightgreen.svg)]()

LifeTree is a lightweight, zero-dependency C++ library for modeling dependency relationships between runtime-owned objects and enforcing safe deletion semantics.

## Capabilities

### Graph Mutation (DAG Enforcement)
- Add and remove modules securely.
- Add logically directed dependency edges (`consumer -> provider`).
- Strict rejection of self-dependencies.
- Strict cycle-rejection on insertion (forces DAG compliance).

### Lifetime Safety
- Pre-compute whether a module can be deleted safely without mutating state (`analyzeDelete`).
- Report direct downstream blockers preventing deletion.
- Analyze transitive blast-radius prior to destructive actions.
- Compute a deterministic dependents-first cascade order (Topological Sorting).
- Explicit safety separation: `unregisterModule(name)` followed by `destroyModule(id)`.

### Deletion Contract
- `deleteModule(name)` remains non-mutating and atomically fails if blocked by active dependents.
- When permitted, `deleteModule(name)` performs unregister + destroy sequentially.
- `unregisterModule(name)` makes a module name-invisible but safely buffers the node via stable `ModuleId`.
- `destroyModule(id)` strictly enforces that nodes are unregistered with zero active dependents before memory is purged.
- `garbageCollect()` safely detects and purges isolated, deferred nodes.

### Lifecycle Observability
- Resolve names to stable integer IDs via `lookupModuleId`.
- Inspect internal node attributes by ID with `getModuleById`.
- Check live namespace availability with `isModuleRegistered`.
- Track active memory footprint with `registeredModuleCount`.

### Inspection and Debugging
- Granular dependency and dependent edge queries (`getDependencies`, `getDependents`).
- Transitive traversal using Depth-First Search (DFS).
- Strict topological ordering guaranteed by `std::set` structures.
- Roots, leaves, isolated-node, and total edge-count helper endpoints.
- Native Graphviz DOT export utilizing resilient `ModuleId`-keyed edges.
- Deterministic JSON export for programmatic CI/CD integration.
- Invariant validation engine for internal stress testing.

## Quickstart

```cpp
#include "lifetree.h"
#include <iostream>

int main() {
  lifetree::LifeTree tree;
  std::string error;

  tree.addModule("core.runtime", &error);
  tree.addModule("core.auth", &error);
  tree.addModule("api.gateway", &error);

  tree.addDependency("core.auth", "core.runtime", &error);
  tree.addDependency("api.gateway", "core.auth", &error);

  lifetree::DeleteAnalysis analysis;
  tree.analyzeDelete("core.runtime", &analysis, &error);
  
  std::cout << "Can delete runtime safely? " 
            << (analysis.CanSafelyDelete ? "Yes" : "No - Blocked by consumers") << "\n";

  std::cout << tree.toDot();
  return 0;
}
```

## Project Layout

```text
lifetree/
|-- CMakeLists.txt
|-- README.md
|-- TEST_RESULTS.md
|-- BENCHMARK_RESULTS.md
|-- benchmarks/
|   |-- lifetree_bench.cpp
|-- include/
|   |-- lifetree.h
|-- src/
|   |-- lifetree.cpp
|-- tests/
|   |-- lifetree_tests.cpp
|   |-- lifetree_stress_tests.cpp
```

## Build And Run

### CMake

```bash
cmake -S . -B build
cmake --build build

ctest --test-dir build --output-on-failure
./build/lifetree_bench
```

### Direct Compiler Invocation

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/lifetree.cpp tests/lifetree_tests.cpp -o build/lifetree_tests
./build/lifetree_tests
```

### Benchmarks

Execute the optimized benchmark configuration:
```bash
g++ -O2 -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/lifetree.cpp benchmarks/lifetree_bench.cpp -o build/lifetree_bench
./build/lifetree_bench
```
*(Verified baseline metrics are available in `BENCHMARK_RESULTS.md`.)*

## Test Coverage

The test suite rigorously verifies 17 distinct operational domains:

1. Base module addition, duplicate rejection, and empty-string handling.
2. Linear dependency structural deletion constraints.
3. Complex diamond dependency DAG traversal.
4. Mathematical cycle insertion detection and rejection.
5. Absolute topological ordering consistency.
6. Missing-node and invalid ID error diagnostics.
7. Dependency edge removal and memory decrements.
8. Transitive search accuracy (DFS).
9. Predictive `analyzeDelete()` non-mutation correctness.
10. System-level `forceDeleteWithCascade` tracking.
11. Statistical extraction APIs.
12. Deep invariant validation constraints.
13. `unregister` and `destroy` deferred lifecycle boundaries.
14. Safe-mapping logic isolating `ModuleId`s against "Ghost Edge" string shadowing.
15. Garbage Collector sweeps for deferred memory isolation logic.
16. Unload mutation lock contracts during blocked unregister attempts.
17. Heavy randomized mutation fuzzer tracking with fixed RNG seeds.

*(Refer to `TEST_RESULTS.md` for output signatures).*
