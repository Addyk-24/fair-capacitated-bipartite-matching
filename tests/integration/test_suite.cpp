#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../unit/test_support.hpp"
#include "experiment.hpp"
#include "registry.hpp"
#include "suite.hpp"

namespace {

using namespace daa;

std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path.c_str());
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += line[i];
        }
    }
    return out;
}

void testParseConfig() {
    SuiteConfig c;
    const std::string text =
        "# comment\n"
        "name = demo   # trailing comment\n"
        "algos = greedy_reference, weighted\n"
        "repeats = 3\n"
        "agents = 10, 20\n"
        "density = 0.5\n"
        "capacity_mode = low, fixed\n"
        "capacity = 1, 2\n"
        "seed = 1,2,3\n"
        "overwrite = true\n";
    CHECK_EQ(parseSuiteConfig(text, c), std::string(""));
    CHECK_EQ(c.name, std::string("demo"));
    CHECK_EQ(c.algos.size(), static_cast<size_t>(2));
    CHECK_EQ(c.repeats, 3);
    CHECK_EQ(c.agents.size(), static_cast<size_t>(2));
    CHECK_EQ(c.seeds.size(), static_cast<size_t>(3));
    CHECK(c.capacity_mode[1] == CapacityMode::Fixed);
    CHECK(c.overwrite);
    CHECK_EQ(c.raw_text, text);
}

void testParseRejectsMistakes() {
    SuiteConfig c;
    CHECK(!parseSuiteConfig("agnts = 10\n", c).empty());          // typo'd key
    CHECK(!parseSuiteConfig("agents = 10\nagents = 20\n", c).empty());  // duplicate key
    CHECK(!parseSuiteConfig("agents = ten\n", c).empty());
    CHECK(!parseSuiteConfig("agents = 10,,20\n", c).empty());     // empty list item
    CHECK(!parseSuiteConfig("agents\n", c).empty());              // no '='
    CHECK(!parseSuiteConfig("agents =\n", c).empty());            // no value
    CHECK(!parseSuiteConfig("repeats = 0\n", c).empty());
    CHECK(!parseSuiteConfig("name = ../escape\n", c).empty());    // path traversal
    CHECK(!parseSuiteConfig("utility_dist = gaussian\n", c).empty());
    CHECK(!parseSuiteConfig("seed = -1\n", c).empty());
    const std::string err = parseSuiteConfig("agents = 5\nbogus = 1\n", c);
    CHECK(err.find("line 2") != std::string::npos);
}

void testPlanExpandsCrossProduct() {
    SuiteConfig c;
    CHECK_EQ(parseSuiteConfig("agents = 10,20\nresources = 5\ndensity = 0.3,0.6\n"
                              "capacity_mode = low,fixed\ncapacity = 1,2\nseed = 1,2\n", c),
             std::string(""));
    std::vector<PlannedDataset> plan;
    CHECK_EQ(planSuite(c, plan), std::string(""));
    // 2 agents x 1 resources x 2 densities x (low:1 + fixed:2 capacities) x 2 seeds = 24
    CHECK_EQ(plan.size(), static_cast<size_t>(24));
    CHECK_EQ(plan[0].size_dir, std::string("n10_m5"));
    CHECK_EQ(plan[0].tag, std::string("d0.3_ps0_uuniform_clow_g2_gs0_seed1"));

    std::vector<PlannedDataset> again;
    CHECK_EQ(planSuite(c, again), std::string(""));
    CHECK_EQ(again[7].id(), plan[7].id());  // same order every time
}

void testPlanRejectsBadSweeps() {
    std::vector<PlannedDataset> plan;
    SuiteConfig c;
    CHECK_EQ(parseSuiteConfig("density = 1.5\n", c), std::string(""));
    CHECK(!planSuite(c, plan).empty());  // generator validation runs before anything executes

    CHECK_EQ(parseSuiteConfig("agents = 10,10\n", c), std::string(""));
    CHECK(!planSuite(c, plan).empty());  // duplicate dataset in sweep
}

void testRunSuiteEndToEnd() {
    registerBuiltinAlgorithms();
    SuiteConfig c;
    CHECK_EQ(parseSuiteConfig("name = suite_e2e\nout_dir = test_tmp_suite\nalgos = all\nrepeats = 2\n"
                              "agents = 30,60\nresources = 6\ndensity = 0.4\ncapacity_mode = low,balanced\n"
                              "seed = 5\noverwrite = true\nsave_datasets = true\n"
                              "dataset_dir = test_tmp_suite_data\n", c),
             std::string(""));

    std::ostringstream log;
    SuiteSummary s;
    CHECK_EQ(runSuite(c, s, log), std::string(""));
    CHECK_EQ(s.datasets, 4);
    CHECK_EQ(s.invalid_runs, 0);
    const int algoCount = static_cast<int>(algorithmNames().size());
    CHECK_EQ(s.runs, 4 * algoCount * 2);

    // Layout: <out>/<name>/<size>/<tag>/runs.csv + params.txt, plus config_used.cfg.
    const std::string dir = "test_tmp_suite/suite_e2e/n30_m6/d0.4_ps0_uuniform_clow_g2_gs0_seed5";
    const std::vector<std::string> rows = readLines(dir + "/runs.csv");
    CHECK_EQ(rows.size(), static_cast<size_t>(1 + algoCount * 2));
    CHECK_EQ(rows[0], resultsHeader());
    CHECK(!readLines(dir + "/params.txt").empty());
    CHECK(!readLines("test_tmp_suite/suite_e2e/config_used.cfg").empty());
    CHECK(!readLines("test_tmp_suite_data/suite_e2e/n30_m6/d0.4_ps0_uuniform_clow_g2_gs0_seed5/edges.csv").empty());

    // Fair comparison: every algorithm and repeat of a dataset saw the same instance.
    const size_t hashCol = fields(resultsHeader()).size() - 1;
    CHECK_EQ(fields(resultsHeader())[hashCol], std::string("instance_hash"));
    for (size_t i = 2; i < rows.size(); ++i) {
        CHECK_EQ(fields(rows[i])[hashCol], fields(rows[1])[hashCol]);
        CHECK_EQ(fields(rows[i])[3], std::string("30"));  // agents column
    }
    // Different datasets have different fingerprints.
    const std::vector<std::string> other =
        readLines("test_tmp_suite/suite_e2e/n60_m6/d0.4_ps0_uuniform_clow_g2_gs0_seed5/runs.csv");
    CHECK(fields(other[1])[hashCol] != fields(rows[1])[hashCol]);
}

void testReproducibleAcrossRuns() {
    registerBuiltinAlgorithms();
    SuiteConfig c;
    CHECK_EQ(parseSuiteConfig("name = repro\nout_dir = test_tmp_suite\nalgos = weighted\nagents = 40\n"
                              "resources = 8\nseed = 9\noverwrite = true\n", c),
             std::string(""));
    std::ostringstream log;
    SuiteSummary s;
    const std::string csv = "test_tmp_suite/repro/n40_m8/d0.2_ps0_uuniform_cbalanced_g2_gs0_seed9/runs.csv";

    CHECK_EQ(runSuite(c, s, log), std::string(""));
    const std::vector<std::string> first = readLines(csv);
    CHECK_EQ(runSuite(c, s, log), std::string(""));  // overwrite = true replaces the file
    const std::vector<std::string> second = readLines(csv);

    CHECK_EQ(first.size(), static_cast<size_t>(2));
    CHECK_EQ(second.size(), static_cast<size_t>(2));
    // Everything except the measured runtime (column 9) is identical.
    const std::vector<std::string> a = fields(first[1]);
    const std::vector<std::string> b = fields(second[1]);
    for (size_t i = 0; i < a.size(); ++i) {
        if (i != 9) CHECK_EQ(a[i], b[i]);
    }
}

void testRefusesToReplaceRawResults() {
    registerBuiltinAlgorithms();
    SuiteConfig c;
    CHECK_EQ(parseSuiteConfig("name = keep\nout_dir = test_tmp_suite\nalgos = greedy_reference\n"
                              "agents = 20\nresources = 4\noverwrite = true\n", c),
             std::string(""));
    std::ostringstream log;
    SuiteSummary s;
    CHECK_EQ(runSuite(c, s, log), std::string(""));

    c.overwrite = false;
    const std::string err = runSuite(c, s, log);
    CHECK(err.find("already exists") != std::string::npos);
    CHECK_EQ(s.runs, 0);  // refused up front, nothing ran
}

void testUnknownAlgorithmFailsBeforeRunning() {
    registerBuiltinAlgorithms();
    SuiteConfig c;
    CHECK_EQ(parseSuiteConfig("name = unknown_algo\nout_dir = test_tmp_suite\nalgos = nope\n", c),
             std::string(""));
    std::ostringstream log;
    SuiteSummary s;
    CHECK(!runSuite(c, s, log).empty());
    CHECK_EQ(s.runs, 0);
}

void testInvalidAlgorithmIsCountedNotFatal() {
    registerBuiltinAlgorithms();
    registerAlgorithm("zz_cheater", [](const Instance&) {
        MatchingResult r;
        r.assignments = {{0, 0}, {0, 0}};
        r.total_assigned_agents = 2;
        return r;
    });
    SuiteConfig c;
    CHECK_EQ(parseSuiteConfig("name = cheat\nout_dir = test_tmp_suite\nalgos = greedy_reference,zz_cheater\n"
                              "agents = 20\nresources = 4\ndensity = 1\noverwrite = true\n", c),
             std::string(""));
    std::ostringstream log;
    SuiteSummary s;
    CHECK_EQ(runSuite(c, s, log), std::string(""));
    CHECK_EQ(s.runs, 2);
    CHECK_EQ(s.invalid_runs, 1);
    CHECK(log.str().find("INVALID") != std::string::npos);
}

}  // namespace

int main() {
    testParseConfig();
    testParseRejectsMistakes();
    testPlanExpandsCrossProduct();
    testPlanRejectsBadSweeps();
    testRunSuiteEndToEnd();
    testReproducibleAcrossRuns();
    testRefusesToReplaceRawResults();
    testUnknownAlgorithmFailsBeforeRunning();
    testInvalidAlgorithmIsCountedNotFatal();
    return finishTests("test_suite");
}
