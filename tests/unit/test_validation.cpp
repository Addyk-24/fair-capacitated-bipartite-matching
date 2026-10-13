#include <string>

#include "test_support.hpp"
#include "validation.hpp"

namespace {

using namespace daa;

// 3 agents, 2 resources (capacity 1 and 2).
//   agent 0: r0 (10), r1 (5)
//   agent 1: r0 (8),  r1 (7)
//   agent 2: r0 (9)
Instance sample() {
    Instance inst;
    inst.num_agents = 3;
    inst.num_resources = 2;
    inst.num_groups = 2;
    inst.capacity = {1, 2};
    inst.group_id = {0, 1, 1};
    inst.edges = {{0, 0, 10}, {0, 1, 5}, {1, 0, 8}, {1, 1, 7}, {2, 0, 9}};
    return inst;
}

MatchingResult result(const std::vector<std::pair<int32_t, int32_t>>& pairs, int64_t utility) {
    MatchingResult r;
    r.assignments = pairs;
    r.total_assigned_agents = static_cast<int32_t>(pairs.size());
    r.total_utility = utility;
    return r;
}

void testInstanceChecks() {
    CHECK_EQ(validateInstance(sample()), std::string(""));

    Instance bad = sample();
    bad.capacity.pop_back();
    CHECK(!validateInstance(bad).empty());

    bad = sample();
    bad.group_id[1] = 2;  // outside [0, num_groups)
    CHECK(!validateInstance(bad).empty());

    bad = sample();
    bad.capacity[0] = -1;
    CHECK(!validateInstance(bad).empty());

    bad = sample();
    bad.edges.push_back({2, 1, 4});  // valid, still sorted
    CHECK_EQ(validateInstance(bad), std::string(""));
    bad.edges.push_back({2, 1, 4});  // duplicate pair
    CHECK(!validateInstance(bad).empty());

    bad = sample();
    std::swap(bad.edges[0], bad.edges[1]);  // unsorted
    CHECK(!validateInstance(bad).empty());

    bad = sample();
    bad.edges.push_back({3, 0, 1});  // agent out of range
    CHECK(!validateInstance(bad).empty());

    Instance empty;
    CHECK_EQ(validateInstance(empty), std::string(""));
}

void testResultAccepts() {
    const Instance inst = sample();
    CHECK_EQ(validateResult(inst, result({}, 0)), std::string(""));
    CHECK_EQ(validateResult(inst, result({{0, 1}, {1, 1}, {2, 0}}, 5 + 7 + 9)), std::string(""));
}

void testResultRejects() {
    const Instance inst = sample();
    // Agent 0 twice.
    CHECK(!validateResult(inst, result({{0, 0}, {0, 1}}, 15)).empty());
    // Resource 0 has capacity 1 but receives two agents.
    CHECK(!validateResult(inst, result({{0, 0}, {1, 0}}, 18)).empty());
    // Agent 2 is not eligible for resource 1.
    CHECK(!validateResult(inst, result({{2, 1}}, 5)).empty());
    // Ids out of range.
    CHECK(!validateResult(inst, result({{5, 0}}, 0)).empty());
    CHECK(!validateResult(inst, result({{0, -1}}, 0)).empty());
    // Wrong utility total.
    CHECK(!validateResult(inst, result({{0, 0}}, 11)).empty());
    // Wrong assigned count.
    MatchingResult r = result({{0, 0}}, 10);
    r.total_assigned_agents = 2;
    CHECK(!validateResult(inst, r).empty());
}

void testZeroCapacity() {
    Instance inst = sample();
    inst.capacity[0] = 0;
    CHECK_EQ(validateInstance(inst), std::string(""));
    CHECK(!validateResult(inst, result({{0, 0}}, 10)).empty());
    CHECK_EQ(validateResult(inst, result({{0, 1}}, 5)), std::string(""));
}

}  // namespace

int main() {
    testInstanceChecks();
    testResultAccepts();
    testResultRejects();
    testZeroCapacity();
    return finishTests("test_validation");
}
