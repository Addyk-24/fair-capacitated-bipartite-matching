#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace daa {

// Eligible edge from agent u to resource v. An absent edge means "not eligible".
struct Edge {
    int32_t agent_id;
    int32_t resource_id;
    int32_t utility;  // Utility weight w_uv (higher is better)
};

// Common problem instance shared by every algorithm stage.
//
// Agents are 0..num_agents-1, resources are 0..num_resources-1.
// Capacity and group labels are stored once per resource / agent (not per edge)
// so they can never disagree with each other.
struct Instance {
    int32_t num_agents = 0;
    int32_t num_resources = 0;
    int32_t num_groups = 1;
    std::vector<int32_t> capacity;  // size num_resources, c_j >= 0
    std::vector<int32_t> group_id;  // size num_agents, in [0, num_groups)
    std::vector<Edge> edges;        // sorted by (agent_id, resource_id), no duplicates
};

// Standardized allocation output structure across all algorithms.
struct MatchingResult {
    std::string algorithm_name;
    int32_t total_assigned_agents = 0;
    int64_t total_utility = 0;
    double fairness_score = -1.0;  // minimum assigned utility / 100; < 0 means unavailable
    double runtime_milliseconds = 0.0;
    std::vector<std::pair<int32_t, int32_t>> assignments;  // (agent_id, resource_id)
};

}  // namespace daa
