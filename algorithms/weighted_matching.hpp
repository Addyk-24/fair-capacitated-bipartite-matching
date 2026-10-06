#ifndef WEIGHTED_MATCHING_HPP
#define WEIGHTED_MATCHING_HPP

#include <string>
#include <vector>

struct WeightedEdge {
    std::string agent_id;
    std::string resource_id;
    int utility;
};

struct WeightedGraph {
    std::vector<std::string> agents;
    std::vector<std::string> resources;
    std::vector<WeightedEdge> edges;
};

struct Assignment {
    std::string agent_id;
    std::string resource_id;
    int utility;
};

struct WeightedMatchingResult {
    std::vector<Assignment> assignments;
    int assigned_count;
    long long total_utility;
    double runtime_ms;
};

WeightedMatchingResult solveMaximumWeightMatching(
    const WeightedGraph& graph
);

#endif