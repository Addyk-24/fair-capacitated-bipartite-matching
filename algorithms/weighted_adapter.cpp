// Adapter that exposes Kartik's maximum-weight matching (algorithms/weighted_matching.*)
// through the common interface: daa::Instance in, daa::MatchingResult out.
//
// Stage semantics: this is the capacity-1 maximum-weight stage. Every resource is used
// at most once, whatever its capacity, so the result is always feasible for the real
// capacities but ignores spare capacity (the capacitated flow stage is what exploits
// it). Resources with capacity 0 are removed before solving. Utilities must lie in
// 0..100 (solver precondition); anything else makes the solver throw, which the
// runner records as an invalid run.
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>

#include "algorithms.hpp"
#include "weighted_matching.hpp"

namespace daa {

namespace {

// Not std::to_string: it is missing on some MinGW builds (see CONTRIBUTING).
std::string nameFor(char prefix, int32_t id) {
    std::ostringstream out;
    out << prefix << id;
    return out.str();
}
std::string agentName(int32_t id) { return nameFor('A', id); }
std::string resourceName(int32_t id) { return nameFor('R', id); }

int32_t idOf(const std::string& name) {
    return static_cast<int32_t>(std::strtol(name.c_str() + 1, nullptr, 10));
}

}  // namespace

MatchingResult runWeighted(const Instance& inst) {
    WeightedGraph graph;
    graph.agents.reserve(static_cast<size_t>(inst.num_agents));
    graph.resources.reserve(static_cast<size_t>(inst.num_resources));
    graph.edges.reserve(inst.edges.size());

    for (int32_t a = 0; a < inst.num_agents; ++a) graph.agents.push_back(agentName(a));
    for (int32_t r = 0; r < inst.num_resources; ++r) {
        if (inst.capacity[static_cast<size_t>(r)] > 0) graph.resources.push_back(resourceName(r));
    }
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& e = inst.edges[i];
        if (inst.capacity[static_cast<size_t>(e.resource_id)] <= 0) continue;
        WeightedEdge we;
        we.agent_id = agentName(e.agent_id);
        we.resource_id = resourceName(e.resource_id);
        we.utility = e.utility;
        graph.edges.push_back(we);
    }

    const WeightedMatchingResult solved = solveMaximumWeightMatching(graph);

    MatchingResult result;
    result.algorithm_name = "weighted";
    result.assignments.reserve(solved.assignments.size());
    for (size_t i = 0; i < solved.assignments.size(); ++i) {
        result.assignments.push_back(std::make_pair(idOf(solved.assignments[i].agent_id),
                                                    idOf(solved.assignments[i].resource_id)));
        result.total_utility += solved.assignments[i].utility;
    }
    result.total_assigned_agents = static_cast<int32_t>(result.assignments.size());
    return result;
}

}  // namespace daa
