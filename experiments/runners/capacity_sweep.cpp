// capacity_sweep: Week 4 capacity experiments in the common results format.
// For each capacity level and seed, generates a dataset with the shared generator and runs
// weighted (capacity-1 Hungarian), capacitated_flow, capacitated_flow_maxcard and
// greedy_reference, appending one row per run to the raw results CSV. A second sweep
// measures runtime over increasing graph sizes.
// Usage: capacity_sweep [output.csv]   (default results/metrics/capacity_sweep.csv)
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "experiment.hpp"
#include "generator.hpp"
#include "registry.hpp"

namespace {

using namespace daa;

const char* const kAlgorithms[] = {"greedy_reference", "weighted", "capacitated_flow",
                                   "capacitated_flow_maxcard"};

int runAll(const Instance& inst, const std::string& dataset, const std::string& csv) {
    int failures = 0;
    for (size_t i = 0; i < sizeof(kAlgorithms) / sizeof(kAlgorithms[0]); ++i) {
        const AlgorithmFn* fn = findAlgorithm(kAlgorithms[i]);
        if (fn == nullptr) {
            std::cerr << "algorithm not registered: " << kAlgorithms[i] << "\n";
            return 1;
        }
        const RunRecord rec = runOnce(inst, dataset, kAlgorithms[i], *fn, 1);
        if (!rec.valid) {
            ++failures;
            std::cerr << dataset << " / " << kAlgorithms[i] << " invalid: " << rec.validation_error << "\n";
        }
        const std::string err = appendRecord(csv, rec);
        if (!err.empty()) {
            std::cerr << err << "\n";
            return 1;
        }
    }
    return failures;
}

}  // namespace

int main(int argc, char** argv) {
    registerBuiltinAlgorithms();
    const std::string csv = argc > 1 ? argv[1] : "results/metrics/capacity_sweep.csv";
    int failures = 0;

    // Sweep A: effect of capacity (200 agents, 20 resources, density 0.3, 5 seeds).
    const int32_t capacities[] = {1, 2, 4, 6, 8, 10, 12, 15};
    for (size_t c = 0; c < sizeof(capacities) / sizeof(capacities[0]); ++c) {
        for (uint64_t seed = 1; seed <= 5; ++seed) {
            GeneratorConfig cfg;
            cfg.agents = 200;
            cfg.resources = 20;
            cfg.density = 0.3;
            cfg.capacity_mode = CapacityMode::Fixed;
            cfg.fixed_capacity = capacities[c];
            cfg.seed = seed;
            Instance inst;
            const std::string err = generateInstance(cfg, inst);
            if (!err.empty()) {
                std::cerr << err << "\n";
                return 1;
            }
            std::ostringstream name;
            name << "capsweep_c" << capacities[c] << "_s" << seed;
            failures += runAll(inst, name.str(), csv);
        }
    }

    // Sweep B: runtime vs graph size (resources = agents / 5, capacity 5, density 0.3).
    const int32_t sizes[] = {100, 200, 400, 800, 1200, 1600};
    for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); ++k) {
        for (uint64_t seed = 1; seed <= 5; ++seed) {
            GeneratorConfig cfg;
            cfg.agents = sizes[k];
            cfg.resources = sizes[k] / 5;
            cfg.density = 0.3;
            cfg.capacity_mode = CapacityMode::Fixed;
            cfg.fixed_capacity = 5;
            cfg.seed = 100 + seed;
            Instance inst;
            const std::string err = generateInstance(cfg, inst);
            if (!err.empty()) {
                std::cerr << err << "\n";
                return 1;
            }
            std::ostringstream name;
            name << "runtime_n" << sizes[k] << "_s" << seed;
            failures += runAll(inst, name.str(), csv);
        }
    }

    std::cout << "wrote " << csv << " (" << failures << " invalid runs)\n";
    return failures == 0 ? 0 : 1;
}
