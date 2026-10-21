#include <algorithm>
#include <vector>

#include "algorithms.hpp"

namespace daa {
namespace {

Instance atLeastUtility(const Instance& inst, int32_t threshold) {
    Instance filtered = inst;
    filtered.edges.erase(std::remove_if(filtered.edges.begin(), filtered.edges.end(),
        [threshold](const Edge& edge) { return edge.utility < threshold; }), filtered.edges.end());
    return filtered;
}

}  // namespace

double minimumNormalizedUtility(const Instance& inst, const MatchingResult& result) {
    if (result.assignments.empty()) return 0.0;
    int32_t minimum = 101;
    for (size_t i = 0; i < result.assignments.size(); ++i) {
        const std::pair<int32_t, int32_t>& assignment = result.assignments[i];
        std::vector<Edge>::const_iterator edge = std::lower_bound(
            inst.edges.begin(), inst.edges.end(), assignment,
            [](const Edge& e, const std::pair<int32_t, int32_t>& key) {
                return e.agent_id != key.first ? e.agent_id < key.first : e.resource_id < key.second;
            });
        if (edge != inst.edges.end() && edge->agent_id == assignment.first &&
            edge->resource_id == assignment.second) {
            minimum = std::min(minimum, edge->utility);
        }
    }
    return static_cast<double>(minimum) / 100.0;
}

MatchingResult runFairness(const Instance& inst) {
    std::vector<int32_t> utilities;
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& edge = inst.edges[i];
        if (inst.capacity[static_cast<size_t>(edge.resource_id)] > 0) utilities.push_back(edge.utility);
    }
    std::sort(utilities.begin(), utilities.end());
    utilities.erase(std::unique(utilities.begin(), utilities.end()), utilities.end());

    MatchingResult result;
    result.algorithm_name = "fairness";
    if (utilities.empty()) {
        result.fairness_score = 0.0;
        return result;
    }

    const int32_t target = runCapacitatedFlowMaxCardinality(inst).total_assigned_agents;
    if (target == 0) {
        result.fairness_score = 0.0;
        return result;
    }

    size_t low = 0, high = utilities.size();
    while (low + 1 < high) {
        const size_t mid = low + (high - low) / 2;
        if (runCapacitatedFlowMaxCardinality(atLeastUtility(inst, utilities[mid]))
                .total_assigned_agents == target) {
            low = mid;
        } else {
            high = mid;
        }
    }

    result = runCapacitatedFlowMaxCardinality(atLeastUtility(inst, utilities[low]));
    result.algorithm_name = "fairness";
    result.fairness_score = static_cast<double>(utilities[low]) / 100.0;
    return result;
}

}  // namespace daa
