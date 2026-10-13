#pragma once

#include <cstdint>

#include "common.hpp"

namespace daa {

// 64-bit FNV-1a hash over every field of an Instance (group labels, capacities,
// then edges in order). Two instances have the same fingerprint iff they hold the
// same data, so a results file can prove that all algorithms in an experiment ran
// on the identical dataset. Integer-only, so it is the same on every platform.
// Time O(agents + resources + edges).
inline uint64_t instanceFingerprint(const Instance& inst) {
    uint64_t h = 1469598103934665603ULL;
    const auto mix = [&h](int64_t v) {
        h ^= static_cast<uint64_t>(v);
        h *= 1099511628211ULL;
    };
    for (size_t i = 0; i < inst.group_id.size(); ++i) mix(inst.group_id[i]);
    for (size_t i = 0; i < inst.capacity.size(); ++i) mix(inst.capacity[i]);
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        mix(inst.edges[i].agent_id);
        mix(inst.edges[i].resource_id);
        mix(inst.edges[i].utility);
    }
    return h;
}

}  // namespace daa
