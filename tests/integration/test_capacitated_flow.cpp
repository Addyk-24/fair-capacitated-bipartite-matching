// Capacitated min-cost flow stage: hand-solved cases, brute-force optimality on tiny
// random instances, cross-check against the Hungarian weighted stage at capacity 1,
// capacity monotonicity, and the registry/runner path.
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../unit/test_support.hpp"
#include "algorithms.hpp"
#include "experiment.hpp"
#include "generator.hpp"
#include "registry.hpp"
#include "validation.hpp"

namespace {

using namespace daa;

typedef std::vector<std::vector<std::pair<int32_t, int32_t> > > Options;  // per agent: (resource, utility)

struct Best {
    int32_t count;
    int64_t utility;
};

void search(const Options& opts, size_t agent, std::vector<int32_t>& load,
            const std::vector<int32_t>& cap, int32_t count, int64_t util, bool maxCard, Best& best) {
    if (agent == opts.size()) {
        const bool better = maxCard ? (count > best.count || (count == best.count && util > best.utility))
                                    : (util > best.utility);
        if (better) {
            best.count = count;
            best.utility = util;
        }
        return;
    }
    search(opts, agent + 1, load, cap, count, util, maxCard, best);  // leave agent unassigned
    for (size_t k = 0; k < opts[agent].size(); ++k) {
        const size_t r = static_cast<size_t>(opts[agent][k].first);
        if (load[r] >= cap[r]) continue;
        ++load[r];
        search(opts, agent + 1, load, cap, count + 1, util + opts[agent][k].second, maxCard, best);
        --load[r];
    }
}

Best bruteForce(const Instance& inst, bool maxCard) {
    Options opts(static_cast<size_t>(inst.num_agents));
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& e = inst.edges[i];
        opts[static_cast<size_t>(e.agent_id)].push_back(std::make_pair(e.resource_id, e.utility));
    }
    std::vector<int32_t> load(static_cast<size_t>(inst.num_resources), 0);
    Best best = {-1, -1};
    search(opts, 0, load, inst.capacity, 0, 0, maxCard, best);
    return best;
}

// Builds an instance from (agent, resource, utility) triples; edges are sorted for the caller.
Instance makeInstance(int32_t agents, const std::vector<int32_t>& capacity,
                      std::vector<Edge> edges) {
    Instance inst;
    inst.num_agents = agents;
    inst.num_resources = static_cast<int32_t>(capacity.size());
    inst.num_groups = 1;
    inst.capacity = capacity;
    inst.group_id.assign(static_cast<size_t>(agents), 0);
    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        return a.agent_id != b.agent_id ? a.agent_id < b.agent_id : a.resource_id < b.resource_id;
    });
    inst.edges = edges;
    return inst;
}

Edge E(int32_t a, int32_t r, int32_t u) {
    Edge e;
    e.agent_id = a;
    e.resource_id = r;
    e.utility = u;
    return e;
}

// ---------------------------------------------------------------------------------

void testHandSolvedCapacityMatters() {
    // Case A: all three agents love resource 0. Capacity decides how many can have it.
    std::vector<Edge> edgesA;
    edgesA.push_back(E(0, 0, 100));
    edgesA.push_back(E(1, 0, 100));
    edgesA.push_back(E(2, 0, 100));
    edgesA.push_back(E(0, 1, 10));
    edgesA.push_back(E(1, 1, 10));
    std::vector<int32_t> capLow;
    capLow.push_back(1);
    capLow.push_back(1);
    std::vector<int32_t> capHigh;
    capHigh.push_back(3);
    capHigh.push_back(1);

    const Instance low = makeInstance(3, capLow, edgesA);
    const MatchingResult rl = runCapacitatedFlow(low);
    CHECK_EQ(validateResult(low, rl), std::string(""));
    CHECK_EQ(rl.total_assigned_agents, 2);  // agent 2 has only resource 0, which is taken
    CHECK_EQ(rl.total_utility, static_cast<int64_t>(110));

    const Instance high = makeInstance(3, capHigh, edgesA);
    const MatchingResult rh = runCapacitatedFlow(high);
    CHECK_EQ(validateResult(high, rh), std::string(""));
    CHECK_EQ(rh.total_assigned_agents, 3);
    CHECK_EQ(rh.total_utility, static_cast<int64_t>(300));
}

void testObjectivesCanDisagree() {
    // Agent 0 likes r0 (100) and tolerates r1 (1); agent 1 only has r0 (1). Capacity 1 each.
    // Max utility: agent 0 -> r0 and agent 1 unassigned = 100.
    // Max cardinality: agent 0 -> r1, agent 1 -> r0 = 2 agents, utility 2.
    std::vector<Edge> edges;
    edges.push_back(E(0, 0, 100));
    edges.push_back(E(0, 1, 1));
    edges.push_back(E(1, 0, 1));
    std::vector<int32_t> cap;
    cap.push_back(1);
    cap.push_back(1);
    const Instance inst = makeInstance(2, cap, edges);

    const MatchingResult u = runCapacitatedFlow(inst);
    CHECK_EQ(validateResult(inst, u), std::string(""));
    CHECK_EQ(u.total_utility, static_cast<int64_t>(100));
    CHECK_EQ(u.total_assigned_agents, 1);

    const MatchingResult c = runCapacitatedFlowMaxCardinality(inst);
    CHECK_EQ(validateResult(inst, c), std::string(""));
    CHECK_EQ(c.total_assigned_agents, 2);
    CHECK_EQ(c.total_utility, static_cast<int64_t>(2));
}

void testSpareCapacityIsUsedUnlikeCapacityOneStage() {
    // Weighted stage uses each resource once; the flow stage must exploit spare capacity.
    std::vector<Edge> edges;
    for (int32_t a = 0; a < 4; ++a) edges.push_back(E(a, 0, 50 + a));
    std::vector<int32_t> cap;
    cap.push_back(4);
    const Instance inst = makeInstance(4, cap, edges);
    const MatchingResult flow = runCapacitatedFlow(inst);
    const MatchingResult weighted = runWeighted(inst);
    CHECK_EQ(validateResult(inst, flow), std::string(""));
    CHECK_EQ(flow.total_assigned_agents, 4);
    CHECK_EQ(weighted.total_assigned_agents, 1);
    CHECK(flow.total_utility > weighted.total_utility);
}

void testZeroCapacityAndEmpty() {
    std::vector<Edge> edges;
    edges.push_back(E(0, 0, 50));
    edges.push_back(E(0, 1, 10));
    edges.push_back(E(1, 0, 50));
    std::vector<int32_t> cap;
    cap.push_back(0);
    cap.push_back(1);
    const Instance inst = makeInstance(2, cap, edges);
    const MatchingResult r = runCapacitatedFlow(inst);
    CHECK_EQ(validateResult(inst, r), std::string(""));
    CHECK_EQ(r.total_assigned_agents, 1);  // resource 0 has no slots; only agent 0 -> resource 1
    CHECK_EQ(r.total_utility, static_cast<int64_t>(10));

    Instance empty;
    CHECK_EQ(runCapacitatedFlow(empty).total_assigned_agents, 0);
    CHECK_EQ(runCapacitatedFlowMaxCardinality(empty).total_assigned_agents, 0);

    GeneratorConfig cfg;
    cfg.agents = 10;
    cfg.resources = 4;
    cfg.density = 0.0;
    Instance noEdges;
    CHECK_EQ(generateInstance(cfg, noEdges), std::string(""));
    CHECK_EQ(runCapacitatedFlow(noEdges).total_assigned_agents, 0);
}

void testMatchesBruteForceOnRandomTinyInstances() {
    int checked = 0;
    for (uint64_t seed = 1; seed <= 150; ++seed) {
        GeneratorConfig cfg;
        cfg.agents = 2 + static_cast<int32_t>(seed % 6);     // 2..7 agents
        cfg.resources = 1 + static_cast<int32_t>(seed % 5);  // 1..5 resources
        cfg.density = 0.2 + 0.1 * static_cast<double>(seed % 7);
        cfg.capacity_mode = static_cast<CapacityMode>(seed % 4);
        cfg.fixed_capacity = 1 + static_cast<int32_t>(seed % 3);
        cfg.utility_dist = static_cast<UtilityDist>(seed % 4);
        cfg.seed = seed;
        Instance inst;
        CHECK_EQ(generateInstance(cfg, inst), std::string(""));
        if (seed % 5 == 0 && !inst.capacity.empty()) inst.capacity[0] = 0;  // zero-capacity resource

        const MatchingResult u = runCapacitatedFlow(inst);
        CHECK_EQ(validateResult(inst, u), std::string(""));
        CHECK_EQ(u.total_utility, bruteForce(inst, false).utility);

        const MatchingResult c = runCapacitatedFlowMaxCardinality(inst);
        CHECK_EQ(validateResult(inst, c), std::string(""));
        const Best bc = bruteForce(inst, true);
        CHECK_EQ(c.total_assigned_agents, bc.count);
        CHECK_EQ(c.total_utility, bc.utility);
        ++checked;
    }
    CHECK_EQ(checked, 150);
}

void testEqualsHungarianAtCapacityOne() {
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
        const MatchingResult flow = runCapacitatedFlow(inst);
        const MatchingResult hungarian = runWeighted(inst);
        CHECK_EQ(validateResult(inst, flow), std::string(""));
        CHECK_EQ(flow.total_utility, hungarian.total_utility);
    }
}

void testUtilityNeverDropsAsCapacityGrows() {
    GeneratorConfig cfg;
    cfg.agents = 100;
    cfg.resources = 10;
    cfg.density = 0.4;
    cfg.seed = 3;
    Instance inst;
    CHECK_EQ(generateInstance(cfg, inst), std::string(""));
    const int32_t levels[] = {1, 2, 3, 5, 8, 12, 100};
    int64_t previous = -1;
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
        inst.capacity.assign(static_cast<size_t>(inst.num_resources), levels[i]);
        const MatchingResult r = runCapacitatedFlow(inst);
        CHECK_EQ(validateResult(inst, r), std::string(""));
        CHECK(r.total_utility >= previous);
        previous = r.total_utility;
    }
    // With capacity >= A nothing binds, so every agent takes its best edge.
    std::vector<int32_t> best(static_cast<size_t>(inst.num_agents), 0);
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        int32_t& b = best[static_cast<size_t>(inst.edges[i].agent_id)];
        b = std::max(b, inst.edges[i].utility);
    }
    int64_t sum = 0;
    for (size_t i = 0; i < best.size(); ++i) sum += best[i];
    CHECK_EQ(previous, sum);
}

void testRegisteredAndDeterministic() {
    registerBuiltinAlgorithms();
    CHECK(findAlgorithm("capacitated_flow") != nullptr);
    CHECK(findAlgorithm("capacitated_flow_maxcard") != nullptr);

    GeneratorConfig cfg;
    cfg.agents = 120;
    cfg.resources = 15;
    cfg.density = 0.3;
    cfg.capacity_mode = CapacityMode::Balanced;
    cfg.seed = 11;
    Instance inst;
    CHECK_EQ(generateInstance(cfg, inst), std::string(""));
    const RunRecord a = runOnce(inst, "d", "capacitated_flow", *findAlgorithm("capacitated_flow"), 1);
    const RunRecord b = runOnce(inst, "d", "capacitated_flow", *findAlgorithm("capacitated_flow"), 2);
    const RunRecord m = runOnce(inst, "d", "capacitated_flow_maxcard",
                                *findAlgorithm("capacitated_flow_maxcard"), 1);
    CHECK(a.valid && b.valid && m.valid);
    CHECK(a.result.assignments == b.result.assignments);  // deterministic
    CHECK(m.result.total_assigned_agents >= a.result.total_assigned_agents);
    CHECK(a.result.total_utility >= m.result.total_utility);
}

}  // namespace

int main() {
    testHandSolvedCapacityMatters();
    testObjectivesCanDisagree();
    testSpareCapacityIsUsedUnlikeCapacityOneStage();
    testZeroCapacityAndEmpty();
    testMatchesBruteForceOnRandomTinyInstances();
    testEqualsHungarianAtCapacityOne();
    testUtilityNeverDropsAsCapacityGrows();
    testRegisteredAndDeterministic();
    return finishTests("test_capacitated_flow");
}
