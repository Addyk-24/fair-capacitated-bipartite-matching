#include <algorithm>
#include <vector>

#include "algorithms.hpp"

namespace daa {
namespace {

bool augment(int32_t agent, const std::vector<std::vector<int32_t> >& adjacency,
             const std::vector<int32_t>& capacity, std::vector<char>& seen,
             std::vector<int32_t>& owner) {
    const std::vector<int32_t>& neighbors = adjacency[static_cast<size_t>(agent)];
    for (size_t i = 0; i < neighbors.size(); ++i) {
        const int32_t resource = neighbors[i];
        if (capacity[static_cast<size_t>(resource)] == 0 || seen[static_cast<size_t>(resource)]) continue;
        seen[static_cast<size_t>(resource)] = 1;
        if (owner[static_cast<size_t>(resource)] == -1 ||
            augment(owner[static_cast<size_t>(resource)], adjacency, capacity, seen, owner)) {
            owner[static_cast<size_t>(resource)] = agent;
            return true;
        }
    }
    return false;
}

}  // namespace

MatchingResult runCardinality(const Instance& inst) {
    MatchingResult result;
    result.algorithm_name = "cardinality";
    const size_t agents = static_cast<size_t>(inst.num_agents);
    const size_t resources = static_cast<size_t>(inst.num_resources);
    std::vector<std::vector<int32_t> > adjacency(agents);
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        adjacency[static_cast<size_t>(inst.edges[i].agent_id)].push_back(inst.edges[i].resource_id);
    }
    std::vector<int32_t> owner(resources, -1);
    for (size_t a = 0; a < agents; ++a) {
        std::vector<char> seen(resources, 0);
        augment(static_cast<int32_t>(a), adjacency, inst.capacity, seen, owner);
    }
    for (size_t r = 0; r < resources; ++r) {
        if (owner[r] < 0) continue;
        const int32_t a = owner[r];
        result.assignments.push_back(std::make_pair(a, static_cast<int32_t>(r)));
        const std::vector<Edge>& edges = inst.edges;
        std::vector<Edge>::const_iterator it = std::lower_bound(
            edges.begin(), edges.end(), std::make_pair(a, static_cast<int32_t>(r)),
            [](const Edge& edge, const std::pair<int32_t, int32_t>& key) {
                return edge.agent_id != key.first ? edge.agent_id < key.first : edge.resource_id < key.second;
            });
        result.total_utility += it->utility;
    }
    std::sort(result.assignments.begin(), result.assignments.end());
    result.total_assigned_agents = static_cast<int32_t>(result.assignments.size());
    return result;
}

}  // namespace daa
