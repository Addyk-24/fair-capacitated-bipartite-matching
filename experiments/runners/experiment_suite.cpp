// experiment_suite: run a whole experiment (dataset sweep x algorithms x repeats) from
// a config file. See experiments/configs/ for examples and include/suite.hpp for keys.
#include <iostream>
#include <string>
#include <vector>

#include "cli.hpp"
#include "registry.hpp"
#include "suite.hpp"

namespace {

void printUsage() {
    std::cout <<
        "Usage: experiment_suite --config <file> [options]\n"
        "\n"
        "Options:\n"
        "  --config FILE   experiment config (key = value lines, lists are comma separated)\n"
        "  --name NAME     override the experiment name (results folder)\n"
        "  --out DIR       override out_dir (default results/metrics)\n"
        "  --overwrite     replace existing runs.csv files instead of refusing\n"
        "  --dry-run       list the datasets that would be generated and exit\n"
        "  --list          print the registered algorithm names and exit\n"
        "  --help          show this message\n"
        "\n"
        "Results: <out>/<name>/<n_agents>_<m_resources>/<parameter tag>/runs.csv\n";
}

}  // namespace

int main(int argc, char** argv) {
    daa::registerBuiltinAlgorithms();

    daa::Args args;
    const std::string parseError = args.parse(argc, argv);
    if (!parseError.empty()) {
        std::cerr << "error: " << parseError << "\n";
        printUsage();
        return 2;
    }
    if (args.has("help")) {
        printUsage();
        return 0;
    }
    if (args.has("list")) {
        const std::vector<std::string> names = daa::algorithmNames();
        for (size_t i = 0; i < names.size(); ++i) std::cout << names[i] << "\n";
        return 0;
    }
    if (!args.has("config")) {
        std::cerr << "error: --config is required\n";
        printUsage();
        return 2;
    }

    daa::SuiteConfig config;
    std::string err = daa::loadSuiteConfig(args.get("config"), config);
    if (!err.empty()) {
        std::cerr << "error: " << err << "\n";
        return 2;
    }
    if (args.has("name")) config.name = args.get("name");
    if (args.has("out")) config.out_dir = args.get("out");
    if (args.has("overwrite")) config.overwrite = true;

    if (args.has("dry-run")) {
        std::vector<daa::PlannedDataset> plan;
        err = daa::planSuite(config, plan);
        if (!err.empty()) {
            std::cerr << "error: " << err << "\n";
            return 2;
        }
        for (size_t i = 0; i < plan.size(); ++i) std::cout << plan[i].size_dir << "/" << plan[i].tag << "\n";
        std::cout << plan.size() << " dataset(s)\n";
        return 0;
    }

    daa::SuiteSummary summary;
    err = daa::runSuite(config, summary, std::cout);
    if (!err.empty()) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }
    std::cout << "done: " << summary.datasets << " dataset(s), " << summary.runs << " run(s), "
              << summary.invalid_runs << " invalid\n";
    return summary.invalid_runs == 0 ? 0 : 1;
}
