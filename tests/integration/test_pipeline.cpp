// End to end: generate -> write -> read -> run by name -> validate -> raw results CSV.
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "../unit/test_support.hpp"
#include "experiment.hpp"
#include "generator.hpp"
#include "instance_io.hpp"
#include "registry.hpp"
#include "validation.hpp"

namespace {

using namespace daa;

std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path.c_str());
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

// 3 agents, resources with capacity 1 and 2. Greedy takes (0,0,10) first, which
// blocks agents 1 and 2 from resource 0, and ends at 10 + 7 = 17. The best
// assignment is {0->1, 1->1, 2->0} = 5 + 7 + 9 = 21, so this also documents that
// the reference heuristic is not optimal.
Instance handSolved() {
    Instance inst;
    inst.num_agents = 3;
    inst.num_resources = 2;
    inst.num_groups = 1;
    inst.capacity = {1, 2};
    inst.group_id = {0, 0, 0};
    inst.edges = {{0, 0, 10}, {0, 1, 5}, {1, 0, 8}, {1, 1, 7}, {2, 0, 9}};
    return inst;
}

void testRegistry() {
    registerBuiltinAlgorithms();
    CHECK(findAlgorithm("greedy_reference") != nullptr);
    CHECK(findAlgorithm("does_not_exist") == nullptr);

    registerAlgorithm("zz_test_noop", [](const Instance&) { return MatchingResult(); });
    const std::vector<std::string> names = algorithmNames();
    bool sorted = true;
    for (size_t i = 1; i < names.size(); ++i) sorted = sorted && names[i - 1] < names[i];
    CHECK(sorted);
    CHECK(findAlgorithm("zz_test_noop") != nullptr);
}

void testGreedyOnHandSolvedExample() {
    registerBuiltinAlgorithms();
    const Instance inst = handSolved();
    const RunRecord rec = runOnce(inst, "hand", "greedy_reference", *findAlgorithm("greedy_reference"), 1);
    CHECK(rec.valid);
    CHECK_EQ(rec.result.total_assigned_agents, 2);
    CHECK_EQ(rec.result.total_utility, static_cast<int64_t>(17));
    CHECK(rec.result.runtime_milliseconds >= 0.0);
    CHECK_EQ(rec.result.algorithm_name, std::string("greedy_reference"));
}

void testGreedyEdgeCases() {
    registerBuiltinAlgorithms();
    const AlgorithmFn& greedy = *findAlgorithm("greedy_reference");

    Instance empty;  // no agents, no resources
    RunRecord rec = runOnce(empty, "empty", "greedy_reference", greedy, 1);
    CHECK(rec.valid);
    CHECK_EQ(rec.result.total_assigned_agents, 0);

    Instance noEdges = handSolved();
    noEdges.edges.clear();
    rec = runOnce(noEdges, "noedges", "greedy_reference", greedy, 1);
    CHECK(rec.valid);
    CHECK_EQ(rec.result.total_assigned_agents, 0);

    Instance zeroCap = handSolved();
    zeroCap.capacity = {0, 0};
    rec = runOnce(zeroCap, "zerocap", "greedy_reference", greedy, 1);
    CHECK(rec.valid);
    CHECK_EQ(rec.result.total_assigned_agents, 0);
}

void testRunnerFlagsInvalidAlgorithmOutput() {
    const Instance inst = handSolved();
    const AlgorithmFn cheater = [](const Instance&) {
        MatchingResult r;
        r.assignments = {{0, 0}, {1, 0}};  // overloads resource 0 (capacity 1)
        r.total_assigned_agents = 2;
        r.total_utility = 18;
        return r;
    };
    const RunRecord rec = runOnce(inst, "hand", "cheater", cheater, 1);
    CHECK(!rec.valid);
    CHECK(!rec.validation_error.empty());
}

// Guards the benchmark clock: a coarse (~15 ms tick) timer would report exactly 0
// or multiples of the tick for an empty algorithm; a fine one reports tiny
// positive values.
void testTimerResolution() {
    const Instance inst = handSolved();
    const AlgorithmFn noop = [](const Instance&) { return MatchingResult(); };
    bool sawFineReading = false;
    for (int i = 0; i < 50; ++i) {
        const RunRecord rec = runOnce(inst, "hand", "noop", noop, 1);
        const double t = rec.result.runtime_milliseconds;
        if (t > 0.0 && t < 5.0) sawFineReading = true;
    }
    CHECK(sawFineReading);
}

void testFullPipelineAndResultsFile() {
    registerBuiltinAlgorithms();
    const std::string dataDir = "test_tmp_pipeline/dataset_a";
    const std::string csv = "test_tmp_pipeline/out/runs.csv";
    std::remove(csv.c_str());

    GeneratorConfig cfg;
    cfg.agents = 300;
    cfg.resources = 25;
    cfg.density = 0.15;
    cfg.popularity_skew = 1.0;
    cfg.capacity_mode = CapacityMode::Low;
    Instance generated;
    CHECK_EQ(generateInstance(cfg, generated), std::string(""));
    CHECK_EQ(writeInstance(dataDir, generated), std::string(""));

    Instance loaded;
    CHECK_EQ(readInstance(dataDir, loaded), std::string(""));

    const RunRecord a = runOnce(loaded, baseName(dataDir), "greedy_reference",
                                *findAlgorithm("greedy_reference"), 1);
    const RunRecord b = runOnce(loaded, baseName(dataDir), "greedy_reference",
                                *findAlgorithm("greedy_reference"), 2);
    CHECK(a.valid && b.valid);
    CHECK_EQ(a.result.total_utility, b.result.total_utility);  // deterministic
    CHECK(a.result.total_assigned_agents <= 150);  // 300 agents, total capacity 150: it binds

    CHECK_EQ(appendRecord(csv, a), std::string(""));
    CHECK_EQ(appendRecord(csv, b), std::string(""));

    const std::vector<std::string> lines = readLines(csv);
    CHECK_EQ(lines.size(), static_cast<size_t>(3));  // one header, two rows
    if (lines.size() == 3) {
        CHECK_EQ(lines[0], resultsHeader());
        CHECK(lines[1].find("dataset_a,greedy_reference,1,300,25,") == 0);
        CHECK(lines[2].find("dataset_a,greedy_reference,2,300,25,") == 0);
        CHECK(lines[1].find(",NA,") != std::string::npos);  // fairness not computed yet
        CHECK(lines[1][lines[1].size() - 1] == '1');        // valid flag
    }

    std::remove(csv.c_str());
}

}  // namespace

int main() {
    testRegistry();
    testGreedyOnHandSolvedExample();
    testGreedyEdgeCases();
    testRunnerFlagsInvalidAlgorithmOutput();
    testTimerResolution();
    testFullPipelineAndResultsFile();
    // Leave the generated dataset files; they live under the build directory.
    return finishTests("test_pipeline");
}
