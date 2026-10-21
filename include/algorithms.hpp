#pragma once

#include "common.hpp"

// Entry points of every algorithm stage. Implementations live in algorithms/*.cpp
// (picked up automatically by CMake). Each takes the common Instance and returns a
// MatchingResult; set algorithm_name, total_assigned_agents, total_utility and
// assignments. The runner measures runtime itself.
//
// Planned stage names (register in src/registry.cpp):
//   cardinality      - Om     (Week 1-2)
//   weighted         - Kartik (Week 2)
//   capacitated_flow - Yash   (Week 2)
//   fairness         - max-cardinality then max-min utility
namespace daa {

// Maximum-cardinality bipartite matching using augmenting paths. This baseline
// treats every resource as capacity one (capacity zero means unavailable).
// Time O(A * E), space O(A + R + E).
MatchingResult runCardinality(const Instance& inst);

// Maximum-cardinality allocation, then maximum minimum assigned utility. Among
// allocations tied on those objectives, maximize total utility.
MatchingResult runFairness(const Instance& inst);

// Minimum assigned utility normalized to [0,1]; empty allocations score zero.
double minimumNormalizedUtility(const Instance& inst, const MatchingResult& result);

// Reference heuristic: take edges by decreasing utility, assign when the agent is
// still free and the resource has capacity left. Deterministic but NOT optimal.
// It exists so the runner, generator and validators can be exercised before the
// real stages land, and as a sanity lower bound in experiments.
// Time O(E log E), space O(E).
MatchingResult runGreedyReference(const Instance& inst);

// Maximum-weight matching (Hungarian, algorithms/weighted_matching.*) at capacity 1:
// each resource is used at most once regardless of its capacity; zero-capacity
// resources are never used. Requires utilities in 0..100. Adapter: weighted_adapter.cpp.
// Time O(E(A+R) + AR + k^2 l) with k = min(A,R), l = max(A,R); space O(AR).
MatchingResult runWeighted(const Instance& inst);

// Capacity-constrained allocation by min-cost flow (algorithms/capacitated_flow.cpp).
// runCapacitatedFlow maximizes total utility (same objective as runWeighted, but
// resources accept up to capacity[v] agents; agents may stay unassigned).
// runCapacitatedFlowMaxCardinality is classic min-cost MAX-flow: it assigns the maximum
// possible number of agents first, then maximizes utility among those allocations.
// Successive shortest paths with potentials: O(V*E + F*E*log V) time,
// V = A + R + 2, E = edges + A + R,
// space O(V + E).
MatchingResult runCapacitatedFlow(const Instance& inst);
MatchingResult runCapacitatedFlowMaxCardinality(const Instance& inst);

}  // namespace daa
