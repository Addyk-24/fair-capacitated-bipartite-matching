// Runner hooks: exception safety, result checks, fairness metric slot, fingerprint.
#include <stdexcept>
#include <string>

#include "experiment.hpp"
#include "fingerprint.hpp"
#include "test_support.hpp"

namespace {

using namespace daa;

Instance sample() {
    Instance inst;
    inst.num_agents = 3;
    inst.num_resources = 2;
    inst.num_groups = 2;
    inst.capacity = {1, 2};
    inst.group_id = {0, 0, 1};
    inst.edges = {{0, 0, 10}, {0, 1, 5}, {1, 0, 8}, {1, 1, 7}, {2, 0, 9}};
    return inst;
}

MatchingResult onePair(const Instance&) {
    MatchingResult r;
    r.assignments = {{0, 0}};
    r.total_assigned_agents = 1;
    r.total_utility = 10;
    return r;
}

void testFingerprint() {
    const Instance a = sample();
    Instance b = sample();
    CHECK_EQ(instanceFingerprint(a), instanceFingerprint(b));

    b.edges[3].utility = 6;
    CHECK(instanceFingerprint(a) != instanceFingerprint(b));
    b = sample();
    b.capacity[1] = 3;
    CHECK(instanceFingerprint(a) != instanceFingerprint(b));
    b = sample();
    b.group_id[2] = 0;
    CHECK(instanceFingerprint(a) != instanceFingerprint(b));
}

void testRecordCarriesFingerprint() {
    const Instance inst = sample();
    const RunRecord rec = runOnce(inst, "d", "one", onePair, 1);
    CHECK(rec.valid);
    CHECK_EQ(rec.instance_hash, instanceFingerprint(inst));
}

void testExceptionBecomesInvalidRun() {
    const Instance inst = sample();
    const AlgorithmFn thrower = [](const Instance&) -> MatchingResult {
        throw std::runtime_error("boom");
    };
    const RunRecord rec = runOnce(inst, "d", "thrower", thrower, 1);
    CHECK(!rec.valid);
    CHECK(rec.validation_error.find("boom") != std::string::npos);
    CHECK_EQ(rec.result.algorithm_name, std::string("thrower"));
    CHECK_EQ(rec.result.total_assigned_agents, 0);

    const AlgorithmFn weird = [](const Instance&) -> MatchingResult { throw 42; };
    CHECK(!runOnce(inst, "d", "weird", weird, 1).valid);
}

void testResultChecks() {
    const Instance inst = sample();
    clearResultChecks();

    int calls = 0;
    addResultCheck("always_ok", [&calls](const Instance&, const MatchingResult&) {
        ++calls;
        return std::string();
    });
    CHECK(runOnce(inst, "d", "one", onePair, 1).valid);
    CHECK_EQ(calls, 1);

    addResultCheck("needs_two", [](const Instance&, const MatchingResult& r) {
        return r.total_assigned_agents >= 2 ? std::string() : std::string("only one assigned");
    });
    const RunRecord bad = runOnce(inst, "d", "one", onePair, 1);
    CHECK(!bad.valid);
    CHECK_EQ(bad.validation_error, std::string("needs_two: only one assigned"));

    // Built-in validation fails first, so custom checks never see an invalid result.
    calls = 0;
    const AlgorithmFn cheater = [](const Instance&) {
        MatchingResult r;
        r.assignments = {{2, 1}};  // not an eligible edge
        r.total_assigned_agents = 1;
        r.total_utility = 1;
        return r;
    };
    CHECK(!runOnce(inst, "d", "cheater", cheater, 1).valid);
    CHECK_EQ(calls, 0);

    clearResultChecks();
    CHECK(runOnce(inst, "d", "one", onePair, 1).valid);
}

void testFairnessMetricSlot() {
    const Instance inst = sample();
    setFairnessMetric(nullptr);
    CHECK_EQ(runOnce(inst, "d", "one", onePair, 1).result.fairness_score, 0.10);  // built-in minimum utility

    // Stand-in metric, only to prove the plumbing; the real one comes from the fairness stage.
    setFairnessMetric([](const Instance&, const MatchingResult& r) {
        return static_cast<double>(r.total_assigned_agents) / 3.0;
    });
    const RunRecord rec = runOnce(inst, "d", "one", onePair, 1);
    CHECK(rec.result.fairness_score > 0.33 && rec.result.fairness_score < 0.34);

    // A score set by the algorithm itself is kept.
    const AlgorithmFn selfScored = [](const Instance& i) {
        MatchingResult r = onePair(i);
        r.fairness_score = 0.9;
        return r;
    };
    CHECK_EQ(runOnce(inst, "d", "self", selfScored, 1).result.fairness_score, 0.9);

    // Invalid runs get no score.
    const AlgorithmFn invalid = [](const Instance&) {
        MatchingResult r;
        r.assignments = {{2, 1}};
        r.total_assigned_agents = 1;
        return r;
    };
    CHECK(runOnce(inst, "d", "invalid", invalid, 1).result.fairness_score < 0.0);

    setFairnessMetric(nullptr);
}

}  // namespace

int main() {
    testFingerprint();
    testRecordCarriesFingerprint();
    testExceptionBecomesInvalidRun();
    testResultChecks();
    testFairnessMetricSlot();
    return finishTests("test_hooks");
}
