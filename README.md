# Fair and Capacity-Constrained Bipartite Matching

**Course:** CS3212 – Design and Analysis of Algorithms  
**Department:** Department of Computer Engineering, Vishwakarma Institute of Technology, Pune  
**Faculty Guide:** Dr. Pushkar Shripad Joglekar  
**Language:** C++17 / C++20  

---

## 1. Project Overview
This project investigates generalized bipartite matching from agents to resources under multi-dimensional operational constraints. The implementation is developed in pure C++ across four progressive stages:

$$\text{Agents } (U) \longrightarrow \text{Utility } (w_{ij}) \longrightarrow \text{Capacity } (c_j) \longrightarrow \text{Fairness Formulation} \longrightarrow \text{Allocation}$$

### Algorithmic Progression
1. **Maximum-Cardinality Baseline:** Baseline feasibility matching enforcing the one-assignment-per-agent constraint.
2. **Maximum-Weight Matching:** Preference- and utility-aware assignment maximizing total allocation weight.
3. **Capacity-Constrained Matching:** Network flow formulation (Min-Cost Max-Flow) handling multi-capacity resources ($c_j \ge 1$).
4. **Fairness-Aware Optimization:** Constrained or multi-objective optimization balancing efficiency against group or individual fairness metrics.
5. **Empirical Benchmarks:** Scalability profiles, theoretical complexity verification, runtime trade-offs, and allocation distributions.

---

## 2. Team Members & Module Responsibilities

| Roll No. | Member Name | Assigned Module | Working Branch |
| :--- | :--- | :--- | :--- |
| **30** | Nikhil Kardile | Fairness metrics, literature review, metric evaluation | `nikhil/fairness-metrics` |
| **31** | Om Karkele | Cardinality baseline, formal problem modeling, complexity proofs | `om/cardinality-baseline` |
| **33** | Kartik Mandhane | Utility modeling, maximum-weight matching, preference schemas | `kartik/weighted-matching` |
| **35** | Yash Kashid | Resource-capacity modeling, min-cost max-flow network formulation | `yash/capacity-flow` |
| **39** | Aditya Katkar | Repository architecture, dataset generator, benchmark pipeline | `aditya/repo-experiments` |

---

## 3. Build & Compilation Instructions

### Prerequisites
- Modern C++ Compiler: GCC 10+, Clang 11+, or MSVC 2019+ (Supporting C++17)
- CMake 3.20+
- Git

### Build Steps
```bash
# 1. Configure the build directory
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 2. Build all targets (libraries, runner, and tests)
cmake --build build --config Release

# 3. Execute unit test suite
cd build && ctest --output-on-failure


# On Linux/macOS
./build/matching_runner --input data/processed/sample.csv --algo cardinality

# On Windows
.\build\Release\matching_runner.exe --input data\processed\sample.csv --algo cardinality

---

### 5. `CONTRIBUTING.md`

Save this as `CONTRIBUTING.md` in the repository root:

```markdown
# C++ Engineering & Contribution Guidelines

To ensure deterministic behavior, high performance, and error-free pipeline integration, all contributors must adhere to these standards.

---

## 1. Branch Strategy & Workflow
- `main`: Production-ready, verified code. Merges require all member approvals.
- `develop`: Primary integration branch. All feature branches branch off and merge back here.
- Member Branches:
  - Nikhil: `nikhil/fairness-metrics`
  - Om: `om/cardinality-baseline`
  - Kartik: `kartik/weighted-matching`
  - Yash: `yash/capacity-flow`
  - Aditya: `aditya/repo-experiments`

---

## 2. Algorithmic Constraints & Code Rules
1. **No External Core Solvers:** Core graph matching and flow algorithms must be written from scratch using standard C++ STL containers (`std::vector`, `std::queue`, `std::pair`, etc.). Using external black-box optimization solvers (e.g., Boost.Graph matching wrappers, external LP solvers) for the foundational stages is strictly prohibited.
2. **Deterministic Outputs:** Ensure sorting, tie-breaking, and node traversals are deterministic to guarantee reproducible unit tests.
3. **Memory Management & Performance:**
   - Avoid manual memory management (`new`/`delete`). Use RAII, stack allocation, or standard smart pointers (`std::unique_ptr`).
   - Pass large graph representations, vectors, and string identifiers by `const` reference (`const std::vector<T>&`).
   - Reserve container capacity upfront (`vec.reserve(n)`) when graph sizes are known.
4. **Complexity Declarations:** Every pull request implementing an algorithm must document:
   - Worst-case Time Complexity in Big-$O$ notation.
   - Auxiliary Space Complexity.
   - Edge cases verified (empty graphs, disjoint components, zero-capacity nodes).

---

## 3. Commit Message Convention
Follow Conventional Commits:

- `feat(cardinality)`: Implement BFS/DFS augmenting path search.
- `fix(flow)`: Prevent capacity underflow in backward residual edges.
- `test(weighted)`: Add bipartite graphs with non-uniform agent utilities.
- `perf(generator)`: Optimize synthetic edge insertion using adjacency lists.

