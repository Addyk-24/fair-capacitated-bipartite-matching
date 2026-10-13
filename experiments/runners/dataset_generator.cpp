// dataset_generator: write a reproducible synthetic dataset directory.
#include <iostream>
#include <string>

#include "cli.hpp"
#include "generator.hpp"
#include "instance_io.hpp"

namespace {

void printUsage() {
    std::cout <<
        "Usage: dataset_generator --out <dir> [options]\n"
        "\n"
        "Options (defaults in brackets):\n"
        "  --out DIR              output dataset directory (required)\n"
        "  --agents N             number of agents [100]\n"
        "  --resources M          number of resources [20]\n"
        "  --density P            eligibility probability per pair, 0..1 [0.2]\n"
        "  --popularity-skew S    0 = uniform; larger = low-index resources are eligible\n"
        "                         for many more agents [0]\n"
        "  --utility-dist D       uniform | bell | low | high [uniform]\n"
        "  --utility-min U        smallest utility [1]\n"
        "  --utility-max U        largest utility [100]\n"
        "  --capacity-mode M      low | balanced | high | fixed [balanced]\n"
        "                         (total capacity ~ 0.5x / 1x / 2x the number of agents)\n"
        "  --capacity K           every resource gets capacity K (implies --capacity-mode fixed)\n"
        "  --groups G             number of agent groups [2]\n"
        "  --group-skew S         0 = equal group sizes; larger = group 0 dominates [0]\n"
        "  --seed S               RNG seed, same seed => same dataset [42]\n"
        "  --help                 show this message\n";
}

bool readInt(const daa::Args& args, const std::string& key, int32_t& target) {
    if (!args.has(key)) return true;
    int64_t v = 0;
    if (!daa::parseInt64(args.get(key), v) || v < INT32_MIN || v > INT32_MAX) {
        std::cerr << "error: --" << key << " must be a 32-bit integer\n";
        return false;
    }
    target = static_cast<int32_t>(v);
    return true;
}

bool readDouble(const daa::Args& args, const std::string& key, double& target) {
    if (!args.has(key)) return true;
    if (!daa::parseDouble(args.get(key), target)) {
        std::cerr << "error: --" << key << " must be a number\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
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
    if (!args.has("out")) {
        std::cerr << "error: --out is required\n";
        printUsage();
        return 2;
    }

    daa::GeneratorConfig cfg;
    bool ok = readInt(args, "agents", cfg.agents) && readInt(args, "resources", cfg.resources) &&
              readDouble(args, "density", cfg.density) &&
              readDouble(args, "popularity-skew", cfg.popularity_skew) &&
              readInt(args, "utility-min", cfg.utility_min) &&
              readInt(args, "utility-max", cfg.utility_max) &&
              readInt(args, "groups", cfg.groups) && readDouble(args, "group-skew", cfg.group_skew);
    if (!ok) return 2;

    if (args.has("utility-dist") && !daa::parseUtilityDist(args.get("utility-dist"), cfg.utility_dist)) {
        std::cerr << "error: --utility-dist must be uniform, bell, low or high\n";
        return 2;
    }
    if (args.has("capacity-mode") &&
        !daa::parseCapacityMode(args.get("capacity-mode"), cfg.capacity_mode)) {
        std::cerr << "error: --capacity-mode must be low, balanced, high or fixed\n";
        return 2;
    }
    if (args.has("capacity")) {
        if (!readInt(args, "capacity", cfg.fixed_capacity)) return 2;
        cfg.capacity_mode = daa::CapacityMode::Fixed;
    }
    if (args.has("seed")) {
        int64_t seed = 0;
        if (!daa::parseInt64(args.get("seed"), seed) || seed < 0) {
            std::cerr << "error: --seed must be a non-negative integer\n";
            return 2;
        }
        cfg.seed = static_cast<uint64_t>(seed);
    }

    daa::Instance inst;
    std::string err = daa::generateInstance(cfg, inst);
    if (!err.empty()) {
        std::cerr << "error: " << err << "\n";
        return 2;
    }

    const std::string out = args.get("out");
    err = daa::writeInstance(out, inst, daa::describeConfig(cfg));
    if (!err.empty()) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }
    std::cout << "wrote " << out << ": " << inst.num_agents << " agents, " << inst.num_resources
              << " resources, " << inst.edges.size() << " edges (seed " << cfg.seed << ")\n";
    return 0;
}
