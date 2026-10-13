#pragma once

#include <cstdint>
#include <string>

#include "common.hpp"

namespace daa {

// Shape of the utility values drawn for each eligible edge, all on
// [utility_min, utility_max]. Integer-only so seeds reproduce everywhere.
enum class UtilityDist {
    Uniform,  // flat
    Bell,     // mean of 3 uniform draws, concentrated around the middle
    Low,      // min of 3 uniform draws, most edges have low utility
    High      // max of 3 uniform draws, most edges have high utility
};

// How resource capacities are chosen relative to the number of agents.
enum class CapacityMode {
    Low,       // total capacity ~ 0.5 * agents: scarce, not everyone can be placed
    Balanced,  // total capacity ~ 1.0 * agents
    High,      // total capacity ~ 2.0 * agents: capacity rarely binds
    Fixed      // every resource gets fixed_capacity
};

struct GeneratorConfig {
    int32_t agents = 100;
    int32_t resources = 20;
    double density = 0.2;           // base probability that an (agent, resource) pair is eligible
    double popularity_skew = 0.0;   // 0 = every resource equally popular, larger = a few resources
                                    // are eligible for many more agents (resource j ~ 1/(j+1)^skew)
    UtilityDist utility_dist = UtilityDist::Uniform;
    int32_t utility_min = 1;
    int32_t utility_max = 100;
    CapacityMode capacity_mode = CapacityMode::Balanced;
    int32_t fixed_capacity = 1;     // only used when capacity_mode == Fixed
    int32_t groups = 2;
    double group_skew = 0.0;        // 0 = equal-sized groups, larger = group k ~ 1/(k+1)^skew
    uint64_t seed = 42;
};

// Name <-> enum helpers for the command line and for dataset metadata.
// parse* return false when the name is unknown.
bool parseUtilityDist(const std::string& name, UtilityDist& out);
bool parseCapacityMode(const std::string& name, CapacityMode& out);
std::string toString(UtilityDist d);
std::string toString(CapacityMode m);

// "key=value" lines holding every field of the config, enough to regenerate the
// dataset. Stored as config.txt next to generated datasets.
std::string describeConfig(const GeneratorConfig& cfg);

// Returns "" if the config is usable, otherwise a human-readable reason.
std::string validateConfig(const GeneratorConfig& cfg);

// Builds a reproducible instance: the same config (including seed) always
// yields the same instance, on any compiler or platform.
//
// Generated capacities are always >= 1. When the requested total capacity is
// smaller than the number of resources (tiny agent counts) the total is
// therefore rounded up to one per resource.
//
// Time O(agents * resources), space O(edges).
// Returns "" on success, otherwise the validateConfig error.
std::string generateInstance(const GeneratorConfig& cfg, Instance& out);

}  // namespace daa
