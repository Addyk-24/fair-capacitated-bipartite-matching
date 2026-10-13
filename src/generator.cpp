#include "generator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

#include "rng.hpp"

namespace daa {

namespace {

// weights[k] = (k+1)^-skew, rescaled so the mean weight is exactly 1.
std::vector<double> skewWeights(int32_t count, double skew) {
    std::vector<double> w(static_cast<size_t>(count));
    double sum = 0.0;
    for (int32_t k = 0; k < count; ++k) {
        w[static_cast<size_t>(k)] = std::pow(static_cast<double>(k + 1), -skew);
        sum += w[static_cast<size_t>(k)];
    }
    for (size_t k = 0; k < w.size(); ++k) w[k] = w[k] * static_cast<double>(count) / sum;
    return w;
}

int32_t drawUtility(Rng& rng, UtilityDist dist, int32_t lo, int32_t hi) {
    switch (dist) {
        case UtilityDist::Uniform:
            return rng.nextInt(lo, hi);
        case UtilityDist::Bell: {
            const int64_t s = static_cast<int64_t>(rng.nextInt(lo, hi)) + rng.nextInt(lo, hi) +
                              rng.nextInt(lo, hi);
            return static_cast<int32_t>(s / 3);
        }
        case UtilityDist::Low: {
            int32_t m = rng.nextInt(lo, hi);
            m = std::min(m, rng.nextInt(lo, hi));
            return std::min(m, rng.nextInt(lo, hi));
        }
        case UtilityDist::High: {
            int32_t m = rng.nextInt(lo, hi);
            m = std::max(m, rng.nextInt(lo, hi));
            return std::max(m, rng.nextInt(lo, hi));
        }
    }
    return lo;
}

std::vector<int32_t> makeGroups(Rng& rng, const GeneratorConfig& cfg) {
    std::vector<double> cumulative = skewWeights(cfg.groups, cfg.group_skew);
    double running = 0.0;
    for (size_t k = 0; k < cumulative.size(); ++k) {
        running += cumulative[k];
        cumulative[k] = running;
    }
    std::vector<int32_t> group(static_cast<size_t>(cfg.agents));
    for (int32_t a = 0; a < cfg.agents; ++a) {
        const double r = rng.nextDouble() * running;
        int32_t g = 0;
        while (g + 1 < cfg.groups && r >= cumulative[static_cast<size_t>(g)]) ++g;
        group[static_cast<size_t>(a)] = g;
    }
    return group;
}

std::vector<int32_t> makeCapacities(Rng& rng, const GeneratorConfig& cfg) {
    const size_t m = static_cast<size_t>(cfg.resources);
    if (cfg.capacity_mode == CapacityMode::Fixed) return std::vector<int32_t>(m, cfg.fixed_capacity);

    double factor = 1.0;
    if (cfg.capacity_mode == CapacityMode::Low) factor = 0.5;
    if (cfg.capacity_mode == CapacityMode::High) factor = 2.0;
    const int64_t total = static_cast<int64_t>(std::floor(factor * cfg.agents + 0.5));

    std::vector<int32_t> cap(m, static_cast<int32_t>(total / cfg.resources));
    // Hand out the remainder one unit at a time to distinct random resources
    // (partial Fisher-Yates shuffle of the resource indices).
    std::vector<int32_t> order(m);
    for (size_t j = 0; j < m; ++j) order[j] = static_cast<int32_t>(j);
    const int64_t remainder = total % cfg.resources;
    for (int64_t i = 0; i < remainder; ++i) {
        const int32_t pick = rng.nextInt(static_cast<int32_t>(i), cfg.resources - 1);
        std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(pick)]);
        ++cap[static_cast<size_t>(order[static_cast<size_t>(i)])];
    }
    for (size_t j = 0; j < m; ++j) cap[j] = std::max<int32_t>(cap[j], 1);
    return cap;
}

}  // namespace

bool parseUtilityDist(const std::string& name, UtilityDist& out) {
    if (name == "uniform") out = UtilityDist::Uniform;
    else if (name == "bell") out = UtilityDist::Bell;
    else if (name == "low") out = UtilityDist::Low;
    else if (name == "high") out = UtilityDist::High;
    else return false;
    return true;
}

bool parseCapacityMode(const std::string& name, CapacityMode& out) {
    if (name == "low") out = CapacityMode::Low;
    else if (name == "balanced") out = CapacityMode::Balanced;
    else if (name == "high") out = CapacityMode::High;
    else if (name == "fixed") out = CapacityMode::Fixed;
    else return false;
    return true;
}

std::string toString(UtilityDist d) {
    switch (d) {
        case UtilityDist::Uniform: return "uniform";
        case UtilityDist::Bell: return "bell";
        case UtilityDist::Low: return "low";
        case UtilityDist::High: return "high";
    }
    return "uniform";
}

std::string toString(CapacityMode m) {
    switch (m) {
        case CapacityMode::Low: return "low";
        case CapacityMode::Balanced: return "balanced";
        case CapacityMode::High: return "high";
        case CapacityMode::Fixed: return "fixed";
    }
    return "balanced";
}

std::string describeConfig(const GeneratorConfig& cfg) {
    std::ostringstream meta;
    meta << "agents=" << cfg.agents << "\nresources=" << cfg.resources
         << "\ndensity=" << cfg.density << "\npopularity_skew=" << cfg.popularity_skew
         << "\nutility_dist=" << toString(cfg.utility_dist)
         << "\nutility_min=" << cfg.utility_min << "\nutility_max=" << cfg.utility_max
         << "\ncapacity_mode=" << toString(cfg.capacity_mode)
         << "\nfixed_capacity=" << cfg.fixed_capacity << "\ngroups=" << cfg.groups
         << "\ngroup_skew=" << cfg.group_skew << "\nseed=" << cfg.seed << "\n";
    return meta.str();
}

std::string validateConfig(const GeneratorConfig& cfg) {
    if (cfg.agents < 0) return "agents must be >= 0";
    if (cfg.resources < 0) return "resources must be >= 0";
    if (!(cfg.density >= 0.0 && cfg.density <= 1.0)) return "density must be in [0, 1]";
    if (!(cfg.popularity_skew >= 0.0)) return "popularity-skew must be >= 0";
    if (cfg.utility_min < 0) return "utility-min must be >= 0";
    if (cfg.utility_max < cfg.utility_min) return "utility-max must be >= utility-min";
    if (cfg.fixed_capacity < 0) return "capacity must be >= 0";
    if (cfg.groups < 1) return "groups must be >= 1";
    if (!(cfg.group_skew >= 0.0)) return "group-skew must be >= 0";
    return "";
}

std::string generateInstance(const GeneratorConfig& cfg, Instance& out) {
    const std::string err = validateConfig(cfg);
    if (!err.empty()) return err;

    Rng rng(cfg.seed);
    Instance inst;
    inst.num_agents = cfg.agents;
    inst.num_resources = cfg.resources;
    inst.num_groups = cfg.groups;

    // Draw order is fixed (groups, capacities, edges) so that changing one
    // parameter does not silently reshuffle unrelated parts of the dataset.
    inst.group_id = makeGroups(rng, cfg);
    if (cfg.resources > 0) inst.capacity = makeCapacities(rng, cfg);

    if (cfg.resources > 0) {
        const std::vector<double> popularity = skewWeights(cfg.resources, cfg.popularity_skew);
        inst.edges.reserve(static_cast<size_t>(static_cast<double>(cfg.agents) * cfg.resources *
                                               std::min(1.0, cfg.density) * 1.1) + 16);
        for (int32_t a = 0; a < cfg.agents; ++a) {
            for (int32_t r = 0; r < cfg.resources; ++r) {
                const double p = std::min(1.0, cfg.density * popularity[static_cast<size_t>(r)]);
                if (rng.nextDouble() < p) {
                    inst.edges.push_back({a, r, drawUtility(rng, cfg.utility_dist, cfg.utility_min,
                                                            cfg.utility_max)});
                }
            }
        }
    }

    out = std::move(inst);
    return "";
}

}  // namespace daa
