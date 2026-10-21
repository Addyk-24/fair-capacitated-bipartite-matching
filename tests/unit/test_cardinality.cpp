#include <algorithm>
#include <cstdint>
#include <vector>

#include "../unit/test_support.hpp"
#include "algorithms.hpp"
#include "registry.hpp"
#include "validation.hpp"

namespace {
using namespace daa;

int brute(const Instance& inst, int32_t agent, std::vector<char>& used) {
    if (agent == inst.num_agents) return 0;
    int best = brute(inst, agent + 1, used);
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& e = inst.edges[i];
        if (e.agent_id != agent || inst.capacity[e.resource_id] == 0 || used[e.resource_id]) continue;
        used[e.resource_id] = 1;
        best = std::max(best, 1 + brute(inst, agent + 1, used));
        used[e.resource_id] = 0;
    }
    return best;
}

void testEmptyAndZeroCapacity() {
    Instance empty;
    MatchingResult r = runCardinality(empty);
    CHECK_EQ(r.total_assigned_agents, 0);
    Instance inst;
    inst.num_agents = 3; inst.num_resources = 2; inst.capacity = {0, 1};
    inst.group_id = {0, 0, 0}; inst.edges = {{0, 0, 99}, {1, 1, 4}};
    r = runCardinality(inst);
    CHECK_EQ(r.total_assigned_agents, 1);
    CHECK_EQ(r.total_utility, static_cast<int64_t>(4));
    CHECK_EQ(validateResult(inst, r), std::string(""));
}

void testAugmentingPathAndUtility() {
    Instance inst;
    inst.num_agents = 3; inst.num_resources = 2; inst.capacity = {1, 1};
    inst.group_id = {0, 0, 0};
    // Taking 0->0 must be undone along 1->0->0->1 to reach cardinality two.
    inst.edges = {{0, 0, 2}, {0, 1, 11}, {1, 0, 17}, {2, 1, 99}};
    MatchingResult r = runCardinality(inst);
    CHECK_EQ(r.total_assigned_agents, 2);
    CHECK_EQ(r.total_utility, static_cast<int64_t>(28));
    CHECK_EQ(validateResult(inst, r), std::string(""));
}

void testExhaustiveSmallGraphs() {
    // Every graph on 2x3 vertices, including missing/isolated edges; utility values
    // vary independently and capacities include both zero and positive values.
    for (unsigned mask = 0; mask < 64; ++mask) {
        Instance inst;
        inst.num_agents = 2; inst.num_resources = 3; inst.num_groups = 1;
        inst.capacity = {(mask & 1) ? 1 : 0, (mask & 2) ? 2 : 1, (mask & 4) ? 0 : 1};
        inst.group_id = {0, 0};
        for (int a = 0; a < 2; ++a) for (int r = 0; r < 3; ++r) {
            const unsigned bit = 1u << (3 * a + r);
            if (mask & bit) inst.edges.push_back(Edge{a, r, static_cast<int32_t>(1 + ((mask + a + r) % 100))});
        }
        std::vector<char> used(3, 0);
        const MatchingResult result = runCardinality(inst);
        CHECK_EQ(result.total_assigned_agents, brute(inst, 0, used));
        CHECK_EQ(validateResult(inst, result), std::string(""));
    }
}

void testRegistry() {
    registerBuiltinAlgorithms();
    CHECK(findAlgorithm("cardinality") != nullptr);
}
}

int main() {
    testEmptyAndZeroCapacity();
    testAugmentingPathAndUtility();
    testExhaustiveSmallGraphs();
    testRegistry();
    return finishTests("test_cardinality");
}
