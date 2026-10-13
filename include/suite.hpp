#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "generator.hpp"

namespace daa {

// An experiment = a list of dataset parameter values (a sweep) x a list of
// algorithms x repeats. Every parameter is a comma-separated list in the config
// file; the suite runs the full cross product, so
//   agents = 100,500
//   seed   = 1,2,3
// means 6 datasets. All algorithms see the identical Instance of a dataset.
struct SuiteConfig {
    std::string name = "experiment";   // results go under <out_dir>/<name>/
    std::vector<std::string> algos;    // empty or {"all"}: every registered algorithm
    int32_t repeats = 1;
    std::string out_dir = "results/metrics";
    bool save_datasets = false;        // also write each dataset under dataset_dir/<name>/
    std::string dataset_dir = "data/synthetic";
    bool overwrite = false;            // allow replacing existing runs.csv files

    std::vector<int32_t> agents = {100};
    std::vector<int32_t> resources = {20};
    std::vector<double> density = {0.2};
    std::vector<double> popularity_skew = {0.0};
    std::vector<UtilityDist> utility_dist = {UtilityDist::Uniform};
    int32_t utility_min = 1;
    int32_t utility_max = 100;
    std::vector<CapacityMode> capacity_mode = {CapacityMode::Balanced};
    std::vector<int32_t> capacity = {1};  // values used by capacity_mode = fixed
    std::vector<int32_t> groups = {2};
    std::vector<double> group_skew = {0.0};
    std::vector<uint64_t> seeds = {42};

    std::string raw_text;  // the config text this was parsed from, copied into the results
};

// Parses "key = value" lines ('#' starts a comment). Unknown keys, duplicate keys and
// malformed values are errors, so a typo cannot silently run the wrong experiment.
// Keys: name, algos, repeats, out_dir, save_datasets, dataset_dir, overwrite, agents,
// resources, density, popularity_skew, utility_dist, utility_min, utility_max,
// capacity_mode, capacity, groups, group_skew, seed.
// Returns "" on success, else an error naming the line.
std::string parseSuiteConfig(const std::string& text, SuiteConfig& out);
std::string loadSuiteConfig(const std::string& path, SuiteConfig& out);

// One dataset of the sweep and where its results live:
//   <out_dir>/<name>/<size_dir>/<tag>/runs.csv, e.g. results/metrics/exp/n500_m40/d0.2_...seed7/runs.csv
struct PlannedDataset {
    GeneratorConfig generator;
    std::string size_dir;  // "n500_m40"
    std::string tag;       // remaining parameters, e.g. "d0.2_ps0_uuniform_cbalanced_g2_gs0_seed42"
    std::string id() const { return size_dir + "_" + tag; }  // value of the `dataset` column
};

// Expands the sweep in a fixed order and validates every generator config and tag
// uniqueness before anything runs. Returns "" or an error.
std::string planSuite(const SuiteConfig& config, std::vector<PlannedDataset>& out);

struct SuiteSummary {
    int32_t datasets = 0;
    int32_t runs = 0;
    int32_t invalid_runs = 0;
};

// Runs the whole experiment. Registered algorithms must already be set up
// (registerBuiltinAlgorithms). Writes per dataset: runs.csv (raw rows) and params.txt
// (generator parameters, instance fingerprint, algorithms); per experiment:
// config_used.cfg. Refuses to touch an existing runs.csv unless config.overwrite.
// Progress lines go to `log`. Invalid runs are counted, not fatal.
// Returns "" on success, else an error (nothing is run if planning/checks fail).
std::string runSuite(const SuiteConfig& config, SuiteSummary& summary, std::ostream& log);

}  // namespace daa
