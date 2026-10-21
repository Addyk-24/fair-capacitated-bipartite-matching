
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

struct UtilityMatrix {
    // -1 represents an ineligible agent-resource pair.
    // Eligible utilities must be in the range 0 through 100; -1 is the absent-edge sentinel.
    std::vector<std::vector<int> > values;
    std::vector<std::string> agents;
    std::vector<std::string> resources;
};

struct WeightedMatchingResult {
    std::vector<Assignment> assignments;
    int assigned_count;
    long long total_utility;
    double runtime_ms;
};

// Constructs the utility matrix.
// Throws std::invalid_argument for invalid utilities,
// duplicate edges, or edges referencing unknown IDs.
UtilityMatrix buildUtilityMatrix(const WeightedGraph& graph);

// Maximizes total utility, allowing agents to remain unmatched.
// Every returned pair is eligible, and each agent/resource appears
// in at most one returned assignment.
WeightedMatchingResult solveMaximumWeightMatching(
    const WeightedGraph& graph
);

#endif
