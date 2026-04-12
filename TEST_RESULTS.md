# Test Results

## Last Local Run

Environment: Windows + `g++` (MSYS2 ucrt64)  
Date: 2026-04-12

### Command

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/lifetree.cpp tests/lifetree_tests.cpp -o build/lifetree_tests.exe
.\build\lifetree_tests.exe
```

### Output

```text
[PASS] all LifeTree tests passed
```

## Covered Scenarios

1. Module add + duplicate handling.
2. Linear chain delete safety (`C -> B -> A`).
3. Diamond dependency behavior.
4. Cycle detection on insertion.
5. Topological ordering constraints.
6. Missing-node error handling.
7. Edge removal semantics.
8. Transitive dependency/dependent analysis.
9. Delete analysis report content.
10. Cascade deletion ordering and execution.
11. Roots/leaves/isolated/stats helpers.
12. Internal invariant consistency checks.
13. Unregister/destroy lifecycle semantics.
14. ID-based lifecycle observability semantics.
15. `deleteModule` non-mutation contract when blocked.
