# WasmEdge Dependency Tree Prototype (LFX #4514)

Standalone C++ prototype for the WasmEdge mentorship project:

- Issue: https://github.com/WasmEdge/WasmEdge/issues/4514
- Goal: safe module lifecycle management using explicit dependency tracking

This prototype models module instances as nodes in a directed graph (`dependent -> dependency`) and enforces safe deletion semantics.

## What This Prototype Demonstrates

1. Dependency-aware module lifecycle behavior.
2. Safe deletion checks with clear blocker diagnostics.
3. Cycle prevention during graph updates.
4. Deterministic ordering for load/delete planning.
5. Test-driven validation across realistic dependency shapes.

## Feature Overview

### Core graph operations

- Add/remove modules.
- Add/remove dependency edges.
- Query direct dependencies and dependents.
- Query transitive dependencies and dependents.

### Safety and correctness

- Reject self-dependency edges.
- Reject cycle-creating dependency insertion.
- `canSafelyDelete`: explicit check with blocker list.
- `deleteModule`: safe deletion only when unblocked.
- `forceDeleteWithCascade`: controlled dependents-first cascade deletion.

### Analysis and diagnostics

- `analyzeDelete`: direct + transitive blockers and suggested cascade order.
- `topologicalOrder`: dependency-valid order.
- `validateInvariants`: bidirectional edge consistency checks.
- `stats`/`roots`/`leaves`/`isolatedModules` helpers.
- `toDot`: Graphviz DOT export for visualization.

## Project Layout

```text
wasmedge-dependency-tree-poc/
|-- CMakeLists.txt
|-- README.md
|-- TEST_RESULTS.md
|-- include/
|   |-- dependency_tree.h
|-- src/
|   |-- dependency_tree.cpp
|   |-- main.cpp
|-- tests/
|   |-- test_dependency_tree.cpp
|-- .gitignore
```

## Quick Start

### Option A: CMake (recommended when available)

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/dep_tree_demo
```

### Option B: Direct g++ build

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/dependency_tree.cpp src/main.cpp -o build/dep_tree_demo
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/dependency_tree.cpp tests/test_dependency_tree.cpp -o build/dep_tree_tests
./build/dep_tree_tests
./build/dep_tree_demo
```

## Test Coverage

The current test suite covers:

1. Module add/duplicate/invalid-input behavior.
2. Linear chain deletion constraints.
3. Diamond dependency behavior.
4. Cycle insertion rejection.
5. Topological order constraints.
6. Missing-node error handling.
7. Dependency edge removal semantics.
8. Transitive query correctness.
9. Delete analysis output correctness.
10. Cascade deletion behavior.
11. Graph stats/helper APIs.
12. Invariant validation checks.

Latest local run result is documented in `TEST_RESULTS.md`.

## Example Runtime Output (Demo)

The demo prints:

- topological order,
- graph roots/leaves/stats,
- delete blockers for a target module,
- transitive impact and cascade suggestion,
- DOT graph output,
- post-deletion state transitions.

## Mapping to WasmEdge Integration

Prototype APIs map directly to likely runtime integration points:

- `addModule` / `addDependency`: module instantiate/link steps.
- `canSafelyDelete` / `deleteModule`: store delete guardrails.
- `analyzeDelete`: operator-facing diagnostics for blocked deletion.
- `validateInvariants`: development/debug safety checks.

## Scope Notes

- This is a focused prototype, not a drop-in runtime patch.
- It is intentionally dependency-light for faster review and iteration.
- Primary objective is proving lifecycle safety semantics with runnable evidence.

