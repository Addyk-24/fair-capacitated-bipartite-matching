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
//   fairness         - Week 3
namespace daa {

// Reference heuristic: take edges by decreasing utility, assign when the agent is
// still free and the resource has capacity left. Deterministic but NOT optimal.
// It exists so the runner, generator and validators can be exercised before the
// real stages land, and as a sanity lower bound in experiments.
// Time O(E log E), space O(E).
MatchingResult runGreedyReference(const Instance& inst);

}  // namespace daa
