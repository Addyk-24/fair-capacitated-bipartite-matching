#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "generator.hpp"
#include "test_support.hpp"
#include "validation.hpp"

namespace {

using namespace daa;

bool sameInstance(const Instance& a, const Instance& b) {
    if (a.num_agents != b.num_agents || a.num_resources != b.num_resources ||
        a.num_groups != b.num_groups || a.capacity != b.capacity || a.group_id != b.group_id ||
        a.edges.size() != b.edges.size()) {
        return false;
    }
    for (size_t i = 0; i < a.edges.size(); ++i) {
        if (a.edges[i].agent_id != b.edges[i].agent_id ||
            a.edges[i].resource_id != b.edges[i].resource_id ||
            a.edges[i].utility != b.edges[i].utility) {
            return false;
        }
    }
    return true;
}

int64_t totalCapacity(const Instance& inst) {
    return std::accumulate(inst.capacity.begin(), inst.capacity.end(), static_cast<int64_t>(0));
}

Instance make(const GeneratorConfig& cfg) {
    Instance inst;
    CHECK_EQ(generateInstance(cfg, inst), std::string(""));
    CHECK_EQ(validateInstance(inst), std::string(""));
    return inst;
}

void testDeterminism() {
    GeneratorConfig cfg;
    cfg.agents = 60;
    cfg.resources = 9;
    cfg.density = 0.4;
    cfg.popularity_skew = 1.0;
    cfg.utility_dist = UtilityDist::Bell;

    const Instance a = make(cfg);
    const Instance b = make(cfg);
    CHECK(sameInstance(a, b));

    cfg.seed = 43;
    const Instance c = make(cfg);
    CHECK(!sameInstance(a, c));
}

// Pins one tiny dataset so a platform/compiler difference in generation is caught
// immediately instead of showing up later as "same seed, different results".
void testGoldenFingerprint() {
    GeneratorConfig cfg;
    cfg.agents = 12;
    cfg.resources = 5;
    cfg.density = 0.5;
    cfg.seed = 7;
    const Instance inst = make(cfg);

    uint64_t h = 1469598103934665603ULL;  // FNV-1a over every field
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
    CHECK_EQ(h, 0x7ac9a3d05a593d07ULL);
}

void testCapacityModes() {
    GeneratorConfig cfg;
    cfg.agents = 100;
    cfg.resources = 20;

    cfg.capacity_mode = CapacityMode::Low;
    CHECK_EQ(totalCapacity(make(cfg)), static_cast<int64_t>(50));
    cfg.capacity_mode = CapacityMode::Balanced;
    CHECK_EQ(totalCapacity(make(cfg)), static_cast<int64_t>(100));
    cfg.capacity_mode = CapacityMode::High;
    CHECK_EQ(totalCapacity(make(cfg)), static_cast<int64_t>(200));

    cfg.capacity_mode = CapacityMode::Fixed;
    cfg.fixed_capacity = 3;
    const Instance fixed = make(cfg);
    CHECK(std::all_of(fixed.capacity.begin(), fixed.capacity.end(), [](int32_t c) { return c == 3; }));

    // Total not divisible by the resource count: remainder is spread, nobody below base.
    cfg.capacity_mode = CapacityMode::Balanced;
    cfg.agents = 103;
    const Instance uneven = make(cfg);
    CHECK_EQ(totalCapacity(uneven), static_cast<int64_t>(103));
    CHECK_EQ(*std::min_element(uneven.capacity.begin(), uneven.capacity.end()), 5);
    CHECK_EQ(*std::max_element(uneven.capacity.begin(), uneven.capacity.end()), 6);

    // Fewer agents than resources: generated capacities never drop below 1.
    cfg.agents = 3;
    cfg.capacity_mode = CapacityMode::Low;
    const Instance tiny = make(cfg);
    CHECK_EQ(*std::min_element(tiny.capacity.begin(), tiny.capacity.end()), 1);
}

void testDensityExtremes() {
    GeneratorConfig cfg;
    cfg.agents = 15;
    cfg.resources = 6;

    cfg.density = 0.0;
    CHECK_EQ(make(cfg).edges.size(), static_cast<size_t>(0));
    cfg.density = 1.0;
    CHECK_EQ(make(cfg).edges.size(), static_cast<size_t>(90));
}

void testUtilityDistributions() {
    GeneratorConfig cfg;
    cfg.agents = 200;
    cfg.resources = 10;
    cfg.density = 1.0;
    cfg.utility_min = 10;
    cfg.utility_max = 90;

    double means[4];
    const UtilityDist dists[4] = {UtilityDist::Uniform, UtilityDist::Bell, UtilityDist::Low,
                                  UtilityDist::High};
    for (int d = 0; d < 4; ++d) {
        cfg.utility_dist = dists[d];
        const Instance inst = make(cfg);
        int64_t sum = 0;
        for (size_t i = 0; i < inst.edges.size(); ++i) {
            CHECK(inst.edges[i].utility >= 10 && inst.edges[i].utility <= 90);
            sum += inst.edges[i].utility;
        }
        means[d] = static_cast<double>(sum) / static_cast<double>(inst.edges.size());
    }
    CHECK(means[2] < 40.0);                       // low skews down (expected ~30)
    CHECK(means[3] > 60.0);                       // high skews up (expected ~70)
    CHECK(means[0] > 40.0 && means[0] < 60.0);   // uniform stays near the middle (50)
    CHECK(means[1] > 40.0 && means[1] < 60.0);

    cfg.utility_min = cfg.utility_max = 7;  // degenerate range
    cfg.utility_dist = UtilityDist::Uniform;
    const Instance constant = make(cfg);
    CHECK(std::all_of(constant.edges.begin(), constant.edges.end(),
                      [](const Edge& e) { return e.utility == 7; }));
}

void testGroupsAndSkew() {
    GeneratorConfig cfg;
    cfg.agents = 1000;
    cfg.resources = 4;
    cfg.groups = 3;

    cfg.group_skew = 0.0;
    Instance even = make(cfg);
    CHECK_EQ(even.num_groups, 3);
    std::vector<int> count(3, 0);
    for (size_t a = 0; a < even.group_id.size(); ++a) ++count[even.group_id[a]];
    CHECK(count[0] > 250 && count[1] > 250 && count[2] > 250);

    cfg.group_skew = 2.0;
    Instance skewed = make(cfg);
    std::vector<int> skewCount(3, 0);
    for (size_t a = 0; a < skewed.group_id.size(); ++a) ++skewCount[skewed.group_id[a]];
    CHECK(skewCount[0] > skewCount[1] && skewCount[1] > skewCount[2]);
}

void testPopularitySkew() {
    GeneratorConfig cfg;
    cfg.agents = 500;
    cfg.resources = 10;
    cfg.density = 0.3;
    cfg.popularity_skew = 1.5;

    const Instance inst = make(cfg);
    std::vector<int> degree(10, 0);
    for (size_t i = 0; i < inst.edges.size(); ++i) ++degree[inst.edges[i].resource_id];
    CHECK(degree[0] > 3 * degree[9]);
}

void testEmptyAndInvalidConfigs() {
    GeneratorConfig cfg;
    cfg.agents = 0;
    cfg.resources = 0;
    const Instance empty = make(cfg);
    CHECK_EQ(empty.edges.size(), static_cast<size_t>(0));

    cfg.agents = 5;  // agents but no resources
    const Instance noResources = make(cfg);
    CHECK_EQ(noResources.num_resources, 0);
    CHECK_EQ(noResources.group_id.size(), static_cast<size_t>(5));

    Instance sink;
    GeneratorConfig bad;
    bad.agents = -1;
    CHECK(!generateInstance(bad, sink).empty());
    bad = GeneratorConfig();
    bad.density = 1.5;
    CHECK(!generateInstance(bad, sink).empty());
    bad = GeneratorConfig();
    bad.utility_min = 50;
    bad.utility_max = 10;
    CHECK(!generateInstance(bad, sink).empty());
    bad = GeneratorConfig();
    bad.groups = 0;
    CHECK(!generateInstance(bad, sink).empty());
    bad = GeneratorConfig();
    bad.popularity_skew = -1.0;
    CHECK(!generateInstance(bad, sink).empty());
}

void testEnumParsing() {
    UtilityDist d = UtilityDist::Uniform;
    CHECK(parseUtilityDist("bell", d) && d == UtilityDist::Bell);
    CHECK(!parseUtilityDist("gaussian", d));
    CapacityMode m = CapacityMode::Balanced;
    CHECK(parseCapacityMode("fixed", m) && m == CapacityMode::Fixed);
    CHECK(!parseCapacityMode("huge", m));
    CHECK_EQ(toString(UtilityDist::High), std::string("high"));
    CHECK_EQ(toString(CapacityMode::Low), std::string("low"));
}

}  // namespace

int main() {
    testDeterminism();
    testGoldenFingerprint();
    testCapacityModes();
    testDensityExtremes();
    testUtilityDistributions();
    testGroupsAndSkew();
    testPopularitySkew();
    testEmptyAndInvalidConfigs();
    testEnumParsing();
    return finishTests("test_generator");
}
