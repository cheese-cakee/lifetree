# LifeTree

Dependency-aware lifecycle management for interdependent runtime modules and resources.

LifeTree is a small C++ library for modeling dependency relationships between runtime-owned objects and enforcing safe deletion semantics. It is designed for systems code that needs explicit answers to questions like:

- Can this module be deleted right now?
- Which live objects block deletion?
- What is the minimal safe deletion order?
- Which invariants should always hold after graph mutation?

The focus is correctness, determinism, and inspectability for production lifecycle control paths.

## Why This Exists

Many runtimes, plugin systems, and embedders allow objects to be registered, linked, unregistered, and destroyed at different times. Once dependencies appear between those objects, deletion stops being a bookkeeping problem and becomes a lifetime-safety problem.

LifeTree explores that problem directly with:

- explicit dependency edges (`consumer -> provider`)
- safe-delete checks with blocker diagnostics
- deterministic cascade planning
- cycle rejection on mutation
- invariant validation for debugging and tests

## Capabilities

### Graph mutation

- add and remove modules
- add and remove dependency edges
- reject self-dependencies
- reject cycle-creating insertions

### Lifetime safety

- check whether a module can be deleted safely
- report direct blockers
- analyze transitive impact before deletion
- compute a deterministic dependents-first cascade order
- explicit `unregisterModule(name)` then `destroyModule(id)` flow

### Deletion contract

- `deleteModule(name)` is non-mutating when blocked by active dependents
- when allowed, `deleteModule(name)` performs unregister + destroy atomically
- `unregisterModule(name)` makes a module name-invisible but keeps the node deferred by stable `ModuleId`
- `destroyModule(id)` requires an unregistered node with no active dependents

### Lifecycle observability

- resolve name to stable id with `lookupModuleId`
- inspect node state by id with `getModuleById`
- check lifecycle state with `isModuleRegistered`
- track current name-visible population with `registeredModuleCount`

### Inspection and debugging

- direct dependency and dependent queries
- transitive dependency and dependent traversal
- topological ordering
- roots, leaves, isolated-node, and edge-count helpers
- Graphviz DOT export
- deterministic JSON export for tooling integration
- invariant validation for edge consistency

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
|   |-- demo.cpp
|   |-- runtime_plugin_example.cpp
|-- tests/
|   |-- lifetree_tests.cpp
|   |-- lifetree_stress_tests.cpp
|-- .gitignore
```

## Build And Run

### CMake

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/lifetree_demo
./build/lifetree_runtime_example
./build/lifetree_bench
```

### Benchmark

```bash
g++ -O2 -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/lifetree.cpp benchmarks/lifetree_bench.cpp -o build/lifetree_bench
./build/lifetree_bench
```

Checked-in benchmark notes and baseline numbers are in `BENCHMARK_RESULTS.md`.

### Direct compiler invocation

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/lifetree.cpp src/demo.cpp -o build/lifetree_demo
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/lifetree.cpp tests/lifetree_tests.cpp -o build/lifetree_tests
./build/lifetree_tests
./build/lifetree_demo
```

## Test Coverage

The test suite covers:

1. module add, duplicate handling, and invalid input
2. linear dependency deletion constraints
3. diamond dependency behavior
4. cycle insertion rejection
5. topological ordering constraints
6. missing-node error handling
7. dependency edge removal semantics
8. transitive query correctness
9. delete-analysis output correctness
10. cascade deletion behavior
11. graph stats and helper APIs
12. invariant validation checks
13. unregister/destroy lifecycle semantics
14. id-based lifecycle observability semantics
15. `deleteModule` non-mutation contract when blocked
16. randomized mutation stress coverage with invariant checks (fixed seeds)

The latest local run result is recorded in `TEST_RESULTS.md`.
