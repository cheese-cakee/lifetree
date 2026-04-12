# LifeTree Benchmark Results

Date: 2026-04-12  
Environment: Windows 11 + MSYS2 `g++ 14.2.0`  
Build flags: `-O2 -std=c++17 -Wall -Wextra -Wpedantic`

## Reproducible Run

```bash
g++ -O2 -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/lifetree.cpp benchmarks/lifetree_bench.cpp -o build/lifetree_bench.exe
./build/lifetree_bench.exe
```

The benchmark binary uses fixed seeds:
- mutation seed: `1201`
- analysis graph seed: `2203`
- analysis query seed: `2207`

## Recorded Baseline

| Benchmark | Operations | Total ms | ns/op |
|---|---:|---:|---:|
| `mutation_add_remove_dependency` | 50000 | 15.850 | 316.996 |
| `analysis_analyze_delete` | 15000 | 5621.221 | 374748.067 |

Notes:
- `analysis_analyze_delete` is intentionally heavier; it traverses and computes deterministic cascade information.
- Results should be compared by trend on the same machine/compiler profile, not by absolute value across different environments.
