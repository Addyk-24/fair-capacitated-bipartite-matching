#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace daa {

// Representation of an eligible edge from Agent u to Resource v
struct Edge {
    int32_t agent_id;
    int32_t resource_id;
    int32_t utility;       // Utility weight w_ij (Week 1 Task 3)
    int32_t capacity;      // Resource capacity c_j (Week 1 Task 4)
    int32_t group_id;      // Group label for fairness optimization (Week 1 Task 5)
};

// Standardized allocation output structure across all algorithms
struct MatchingResult {
    std::string algorithm_name;
    int32_t total_assigned_agents;
    int64_t total_utility;
    double fairness_score;
    double runtime_milliseconds;
    std::vector<std::pair<int32_t, int32_t>> assignments; // (agent_id, resource_id)
};

} // namespace daa