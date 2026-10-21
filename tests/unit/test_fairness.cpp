#include <algorithm>
#include <cstdint>
#include <vector>

#include "../unit/test_support.hpp"
#include "algorithms.hpp"
#include "experiment.hpp"
#include "validation.hpp"

namespace {
using namespace daa;

struct Objective { int count; int minimum; };
void enumerate(const Instance& in, int a, std::vector<char>& used, int count, int minimum, Objective& best) {
    if (a == in.num_agents) {
        if (count > best.count || (count == best.count && count > 0 && minimum > best.minimum)) {
            best.count = count; best.minimum = minimum;
        }
        return;
    }
    enumerate(in, a + 1, used, count, minimum, best);
    for (size_t i = 0; i < in.edges.size(); ++i) {
        const Edge& e = in.edges[i];
        if (e.agent_id != a || in.capacity[e.resource_id] == 0 || used[e.resource_id]) continue;
        used[e.resource_id] = 1;
        enumerate(in, a + 1, used, count + 1, std::min(minimum, e.utility), best);
        used[e.resource_id] = 0;
    }
}

void testMaxcardThenMaxmin() {
    Instance in;
    in.num_agents = 3; in.num_resources = 2; in.capacity = {1, 1}; in.group_id = {0, 0, 0};
    in.edges = {{0, 0, 1}, {0, 1, 90}, {1, 0, 85}, {1, 1, 2}, {2, 1, 70}};
    MatchingResult r = runFairness(in);
    CHECK_EQ(r.total_assigned_agents, 2);
    CHECK_EQ(validateResult(in, r), std::string(""));
    CHECK(r.fairness_score >= 0.849);
    CHECK_EQ(r.fairness_score, minimumNormalizedUtility(in, r));
    CHECK_EQ(r.total_utility, static_cast<int64_t>(175));
}

void testBruteForceSmallCases() {
    for (unsigned mask = 0; mask < 64; ++mask) {
        Instance in;
        in.num_agents = 2; in.num_resources = 3; in.capacity = {1, (mask & 1) ? 0 : 1, 1}; in.group_id = {0, 0};
        for (int a = 0; a < 2; ++a) for (int r = 0; r < 3; ++r) {
            if (mask & (1u << (3 * a + r))) in.edges.push_back(Edge{a, r, static_cast<int32_t>(1 + ((mask * 7 + a * 13 + r * 17) % 100))});
        }
        Objective best{0, 0};
        std::vector<char> used(3, 0);
        enumerate(in, 0, used, 0, 101, best);
        const MatchingResult r = runFairness(in);
        CHECK_EQ(r.total_assigned_agents, best.count);
        CHECK_EQ(validateResult(in, r), std::string(""));
        CHECK_EQ(r.fairness_score, best.count ? static_cast<double>(best.minimum) / 100.0 : 0.0);
    }
}

void testEmptyMetricAndRunner() {
    Instance in;
    MatchingResult r = runFairness(in);
    CHECK_EQ(r.fairness_score, 0.0);
    in.num_agents = 1; in.num_resources = 1; in.capacity = {1}; in.group_id = {0};
    in.edges = {{0, 0, 37}};
    const RunRecord rec = runOnce(in, "one", "cardinality", runCardinality, 1);
    CHECK(rec.valid);
    CHECK_EQ(rec.result.fairness_score, 0.37);
}

void testZeroUtilityEligibleEdge() {
    Instance in;
    in.num_agents = 1; in.num_resources = 1; in.capacity = {1}; in.group_id = {0};
    in.edges = {{0, 0, 0}};
    const MatchingResult maxcard = runCapacitatedFlowMaxCardinality(in);
    CHECK_EQ(maxcard.total_assigned_agents, 1);
    CHECK_EQ(validateResult(in, maxcard), std::string(""));
    const MatchingResult fair = runFairness(in);
    CHECK_EQ(fair.total_assigned_agents, 1);
    CHECK_EQ(fair.total_utility, static_cast<int64_t>(0));
    CHECK_EQ(fair.fairness_score, 0.0);
    CHECK_EQ(validateResult(in, fair), std::string(""));
}

void testUnevenAndEmptyDeclaredGroups() {
    Instance in;
    in.num_agents = 3; in.num_resources = 2; in.num_groups = 4;
    in.capacity = {1, 1};
    in.group_id = {0, 0, 3};  // groups 1 and 2 are empty; group 0 is larger than group 3
    in.edges = {{0, 0, 40}, {0, 1, 80}, {1, 0, 70}, {1, 1, 60}, {2, 1, 90}};
    const MatchingResult r = runFairness(in);
    CHECK_EQ(r.total_assigned_agents, 2);
    CHECK_EQ(validateResult(in, r), std::string(""));
    CHECK_EQ(r.fairness_score, 0.70);
}
}

int main() {
    testMaxcardThenMaxmin();
    testBruteForceSmallCases();
    testEmptyMetricAndRunner();
    testZeroUtilityEligibleEdge();
    testUnevenAndEmptyDeclaredGroups();
    return finishTests("test_fairness");
}
