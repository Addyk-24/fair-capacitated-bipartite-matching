// The weighted stage through the common interface: checks the adapter and, via
// brute force on tiny instances, that the Hungarian solver really is optimal.
#include <algorithm>
#include <string>
#include <vector>

#include "../unit/test_support.hpp"
#include "algorithms.hpp"
#include "experiment.hpp"
#include "generator.hpp"
#include "registry.hpp"
#include "validation.hpp"

namespace {

using namespace daa;

// Best total utility when every resource with capacity >= 1 may be used once.
int64_t bruteForce(const Instance& inst) {
    std::vector<std::vector<std::pair<int32_t, int32_t> > > options(
        static_cast<size_t>(inst.num_agents));  // (resource, utility) per agent
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& e = inst.edges[i];
        if (inst.capacity[static_cast<size_t>(e.resource_id)] >= 1) {
            options[static_cast<size_t>(e.agent_id)].push_back(std::make_pair(e.resource_id, e.utility));
        }
    }
    struct Search {
        const std::vector<std::vector<std::pair<int32_t, int32_t> > >& options;
        int64_t best(size_t agent, unsigned used) const {
            if (agent == options.size()) return 0;
            int64_t result = best(agent + 1, used);  // leave this agent unassigned
            for (size_t k = 0; k < options[agent].size(); ++k) {
                const unsigned bit = 1u << options[agent][k].first;
                if (used & bit) continue;
                result = std::max(result, options[agent][k].second + best(agent + 1, used | bit));
            }
            return result;
        }
    };
    const Search search = {options};
    return search.best(0, 0);
}

void testMatchesBruteForceOnRandomTinyInstances() {
    int checked = 0;
    for (uint64_t seed = 1; seed <= 150; ++seed) {
        GeneratorConfig cfg;
        cfg.agents = 2 + static_cast<int32_t>(seed % 6);     // 2..7 agents
        cfg.resources = 1 + static_cast<int32_t>(seed % 5);  // 1..5 resources
        cfg.density = 0.2 + 0.1 * static_cast<double>(seed % 7);
        cfg.capacity_mode = (seed % 2 == 0) ? CapacityMode::Fixed : CapacityMode::Balanced;
        cfg.fixed_capacity = 1;
        cfg.seed = seed;
        cfg.utility_dist = static_cast<UtilityDist>(seed % 4);
        Instance inst;
        CHECK_EQ(generateInstance(cfg, inst), std::string(""));
        if (seed % 5 == 0 && !inst.capacity.empty()) inst.capacity[0] = 0;  // zero-capacity resource

        const MatchingResult r = runWeighted(inst);
        CHECK_EQ(validateResult(inst, r), std::string(""));
        CHECK_EQ(r.total_utility, bruteForce(inst));
        ++checked;
    }
    CHECK_EQ(checked, 150);
}

void testUsesEachResourceAtMostOnceEvenWithSpareCapacity() {
    GeneratorConfig cfg;
    cfg.agents = 60;
    cfg.resources = 6;
    cfg.density = 0.8;
    cfg.capacity_mode = CapacityMode::High;  // capacity ~20 per resource
    Instance inst;
    CHECK_EQ(generateInstance(cfg, inst), std::string(""));

    const MatchingResult r = runWeighted(inst);
    CHECK_EQ(validateResult(inst, r), std::string(""));
    CHECK(r.total_assigned_agents <= 6);  // capacity-1 stage: one agent per resource
}

void testNeverWorseThanGreedyAtUnitCapacity() {
    registerBuiltinAlgorithms();
    for (uint64_t seed = 1; seed <= 20; ++seed) {
        GeneratorConfig cfg;
        cfg.agents = 80;
        cfg.resources = 30;
        cfg.density = 0.2;
        cfg.capacity_mode = CapacityMode::Fixed;
        cfg.fixed_capacity = 1;
        cfg.seed = seed;
        Instance inst;
        CHECK_EQ(generateInstance(cfg, inst), std::string(""));
        const RunRecord w = runOnce(inst, "d", "weighted", *findAlgorithm("weighted"), 1);
        const RunRecord g = runOnce(inst, "d", "greedy_reference", *findAlgorithm("greedy_reference"), 1);
        CHECK(w.valid && g.valid);
        CHECK(w.result.total_utility >= g.result.total_utility);
    }
}

void testEmptyAndEdgeCases() {
    Instance empty;
    CHECK_EQ(runWeighted(empty).total_assigned_agents, 0);

    GeneratorConfig cfg;
    cfg.agents = 10;
    cfg.resources = 4;
    cfg.density = 0.0;
    Instance noEdges;
    CHECK_EQ(generateInstance(cfg, noEdges), std::string(""));
    CHECK_EQ(runWeighted(noEdges).total_assigned_agents, 0);
}

void testUtilityOutsideSolverRangeIsReportedNotCrashed() {
    registerBuiltinAlgorithms();
    GeneratorConfig cfg;
    cfg.agents = 5;
    cfg.resources = 3;
    cfg.density = 1.0;
    cfg.utility_min = 0;  // the weighted solver requires 1..100
    cfg.utility_max = 0;
    Instance inst;
    CHECK_EQ(generateInstance(cfg, inst), std::string(""));

    const RunRecord rec = runOnce(inst, "d", "weighted", *findAlgorithm("weighted"), 1);
    CHECK(!rec.valid);
    CHECK(rec.validation_error.find("threw") != std::string::npos);
}

}  // namespace

int main() {
    testMatchesBruteForceOnRandomTinyInstances();
    testUsesEachResourceAtMostOnceEvenWithSpareCapacity();
    testNeverWorseThanGreedyAtUnitCapacity();
    testEmptyAndEdgeCases();
    testUtilityOutsideSolverRangeIsReportedNotCrashed();
    return finishTests("test_weighted_adapter");
}
