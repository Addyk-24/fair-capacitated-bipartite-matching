#include "weighted_matching.hpp"

WeightedMatchingResult solveMaximumWeightMatching(
    const WeightedGraph& graph
) {
    WeightedMatchingResult result;

    result.assigned_count = 0;
    result.total_utility = 0;
    result.runtime_ms = 0.0;

    return result;
}