# Phase 1

## Goal

Turn the current prototype into a credible standalone library without changing the core problem it solves.

## Keep

- module-name keyed dependency graph
- explicit dependency edges
- safe delete checks
- deterministic cascade planning
- cycle rejection
- invariant validation
- single-header single-library style layout
- C++17 baseline

## Add

- separate logical identity from display name
- explicit unregister versus destroy semantics
- deferred deletion state
- JSON export alongside DOT output
- stronger randomized and stress testing
- benchmark target for mutation and analysis operations
- one integration example beyond the current demo

## Do Not Add In Phase 1

- concurrency beyond the current coarse mutex
- custom allocators
- template-heavy generic graph abstractions
- persistence layers
- GUI or web visualization
- premature multi-runtime adapters

## Done When

- names are no longer the only stable identity
- deletion semantics cover unregister, blocked delete, deferred delete, and final destroy
- tests include randomized mutation coverage in addition to scenario tests
- benchmark numbers are checked into the repo
- README explains the model without reference to WasmEdge or LFX history
