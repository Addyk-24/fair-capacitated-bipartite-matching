#include <algorithm>
#include <vector>

#include "algorithms.hpp"

namespace daa {

MatchingResult runGreedyReference(const Instance& inst) {
    MatchingResult result;
    result.algorithm_name = "greedy_reference";

    // Index edges rather than copying them; ties break on (agent, resource) so the
    // outcome is deterministic.
    std::vector<size_t> order(inst.edges.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&inst](size_t x, size_t y) {
        const Edge& a = inst.edges[x];
        const Edge& b = inst.edges[y];
        if (a.utility != b.utility) return a.utility > b.utility;
        if (a.agent_id != b.agent_id) return a.agent_id < b.agent_id;
        return a.resource_id < b.resource_id;
    });

    std::vector<char> agentUsed(static_cast<size_t>(inst.num_agents), 0);
    std::vector<int32_t> load(static_cast<size_t>(inst.num_resources), 0);
    result.assignments.reserve(static_cast<size_t>(inst.num_agents));

    for (size_t k = 0; k < order.size(); ++k) {
        const Edge& e = inst.edges[order[k]];
        const size_t a = static_cast<size_t>(e.agent_id);
        const size_t r = static_cast<size_t>(e.resource_id);
        if (agentUsed[a] || load[r] >= inst.capacity[r]) continue;
        agentUsed[a] = 1;
        ++load[r];
        result.assignments.push_back(std::make_pair(e.agent_id, e.resource_id));
        result.total_utility += e.utility;
    }
    result.total_assigned_agents = static_cast<int32_t>(result.assignments.size());
    return result;
}

}  // namespace daa
