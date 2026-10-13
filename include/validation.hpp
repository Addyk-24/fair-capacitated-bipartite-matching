#pragma once

#include <string>

#include "common.hpp"

namespace daa {

// Checks the structural invariants documented on Instance: vector sizes, ids in
// range, capacity >= 0, edges strictly sorted by (agent_id, resource_id) which
// also rules out duplicate pairs. Returns "" if valid, else the first problem.
std::string validateInstance(const Instance& inst);

// Checks a result against its instance, independent of how the algorithm
// computed it:
//   - every assignment is an eligible edge,
//   - every agent is assigned at most once,
//   - no resource exceeds its capacity,
//   - total_assigned_agents and total_utility match the assignments.
// Returns "" if valid, else the first violated rule.
// Time O(A log E) for A assignments and E edges, space O(agents + resources).
std::string validateResult(const Instance& inst, const MatchingResult& result);

}  // namespace daa
