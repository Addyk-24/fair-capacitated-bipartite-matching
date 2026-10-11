# C++ Engineering & Contribution Guidelines

To ensure deterministic behavior, high performance, and error-free pipeline integration, all contributors must adhere to these standards.

---

## 1. Branch Strategy & Workflow
- `main`: Production-ready, verified code. Merges require all member approvals.
- `develop`: Primary integration branch. All feature branches branch off and merge back here.
- Member branches (long-lived, one per person):
  - Nikhil: `nikhil/fairness-metrics`
  - Om: `om/cardinality-baseline`
  - Kartik: `kartik/weighted-matching`
  - Yash: `yash/capacity-flow`
  - Aditya: `aditya/repo-experiments`
- For a single task you may branch off your member branch as `<name>/<short-topic>`, e.g. `om/baseline-tests`.
- Never commit directly to `main` or `develop`.

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
5. **One Common Interface:** Algorithms take a `daa::Instance` and return a `daa::MatchingResult` (`include/common.hpp`). Read datasets only through `readInstance`; do not add member-specific input formats. See `docs/specifications/dataset_format.md`, including how to register an algorithm so `matching_runner` can run it by name.
6. **Portable C++17:** No compiler extensions, no `<random>` distributions in anything that must be reproducible (their output differs between compilers; use `include/rng.hpp`). Avoid `printf("%lld")` and `std::to_string` (broken on some MinGW builds); use iostreams.

---

## 3. Commit Message Convention
Follow Conventional Commits:

- `feat(cardinality)`: Implement BFS/DFS augmenting path search.
- `fix(flow)`: Prevent capacity underflow in backward residual edges.
- `test(weighted)`: Add bipartite graphs with non-uniform agent utilities.
- `perf(generator)`: Optimize synthetic edge insertion using adjacency lists.

Scopes in use: `repo`, `generator`, `runner`, `cardinality`, `weighted`, `flow`, `fairness`, `docs`.

---

## 4. Pull Request Rules
- Open the PR from your branch into `develop` (only the release PR goes `develop` → `main`).
- Title follows the commit convention. Fill in the PR description with:
  - what changed and which Week/Task it closes (`Closes #<issue>`),
  - the complexity note from section 2.4 (time, space, edge cases),
  - how you tested it.
- Keep a PR to one task. Small PRs get reviewed faster.
- **Before opening:** the project builds from a clean checkout and `ctest --output-on-failure` passes (see README). Every PR that changes behavior adds or updates tests.
- Use small hand-solvable examples before large experiments, and keep raw experiment data untouched (derive processed files separately).

## 5. Review Rules
- At least **one other member** reviews and approves before merge; the author does not merge their own PR without that approval.
- Reviewers check: correctness against the written spec, determinism, complexity note present, tests cover empty/zero-capacity/disconnected cases, no black-box solver in a core stage, interface unchanged unless the PR says so.
- Changes to shared files (`include/common.hpp`, `src/registry.cpp`, dataset format) need approval from Aditya and must be announced to the team in the PR description.
- Resolve review comments with a new commit; squash-merge into `develop`.
