// matching_runner: load one dataset, run one or more algorithms by name, print a
// summary and append raw rows to a results CSV.
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "cli.hpp"
#include "experiment.hpp"
#include "instance_io.hpp"
#include "registry.hpp"

namespace {

void printUsage() {
    std::cout <<
        "Usage: matching_runner --input <dataset_dir> --algo <name[,name...]|all> [options]\n"
        "\n"
        "Options:\n"
        "  --input DIR     dataset directory (agents.csv, resources.csv, edges.csv)\n"
        "  --algo LIST     comma-separated algorithm names, or 'all'\n"
        "  --output FILE   raw results CSV to append to (default results/metrics/runs.csv)\n"
        "  --repeats N     runs per algorithm, one result row each (default 1)\n"
        "  --list          print the registered algorithm names and exit\n"
        "  --help          show this message\n";
}

std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ',') {
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    if (!cur.empty()) parts.push_back(cur);
    return parts;
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
    if (!args.has("input") || !args.has("algo")) {
        std::cerr << "error: --input and --algo are required\n";
        printUsage();
        return 2;
    }

    int64_t repeats = 1;
    if (args.has("repeats") && (!daa::parseInt64(args.get("repeats"), repeats) || repeats < 1)) {
        std::cerr << "error: --repeats must be a positive integer\n";
        return 2;
    }

    std::vector<std::string> algos = splitList(args.get("algo"));
    if (algos.size() == 1 && algos[0] == "all") algos = daa::algorithmNames();
    if (algos.empty()) {
        std::cerr << "error: --algo is empty\n";
        return 2;
    }
    for (size_t i = 0; i < algos.size(); ++i) {
        if (daa::findAlgorithm(algos[i]) == nullptr) {
            std::cerr << "error: unknown algorithm '" << algos[i] << "'. Registered:";
            const std::vector<std::string> names = daa::algorithmNames();
            for (size_t k = 0; k < names.size(); ++k) std::cerr << " " << names[k];
            std::cerr << "\n";
            return 2;
        }
    }

    const std::string input = args.get("input");
    const std::string output = args.get("output", "results/metrics/runs.csv");

    daa::Instance inst;
    const std::string readError = daa::readInstance(input, inst);
    if (!readError.empty()) {
        std::cerr << "error: " << readError << "\n";
        return 1;
    }
    const std::string datasetName = daa::baseName(input);
    std::cout << "dataset " << datasetName << ": " << inst.num_agents << " agents, "
              << inst.num_resources << " resources, " << inst.edges.size() << " edges\n";

    int exitCode = 0;
    for (size_t i = 0; i < algos.size(); ++i) {
        const daa::AlgorithmFn& fn = *daa::findAlgorithm(algos[i]);
        for (int32_t rep = 1; rep <= repeats; ++rep) {
            const daa::RunRecord rec = daa::runOnce(inst, datasetName, algos[i], fn, rep);
            if (rep == 1) {
                std::cout << "  " << std::left << std::setw(18) << algos[i] << std::right
                          << " assigned=" << rec.result.total_assigned_agents
                          << " utility=" << rec.result.total_utility << " time=" << std::fixed
                          << std::setprecision(3) << rec.result.runtime_milliseconds << " ms "
                          << (rec.valid ? "valid" : "INVALID") << "\n";
            }
            if (!rec.valid) {
                std::cerr << "error: " << algos[i] << " produced an invalid result: "
                          << rec.validation_error << "\n";
                exitCode = 1;
            }
            const std::string writeError = daa::appendRecord(output, rec);
            if (!writeError.empty()) {
                std::cerr << "error: " << writeError << "\n";
                return 1;
            }
        }
    }
    std::cout << "results appended to " << output << "\n";
    return exitCode;
}
