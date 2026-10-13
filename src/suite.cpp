#include "suite.hpp"

#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <utility>

#include "cli.hpp"
#include "experiment.hpp"
#include "fingerprint.hpp"
#include "instance_io.hpp"
#include "registry.hpp"

namespace daa {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> splitList(const std::string& value) {
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i <= value.size(); ++i) {
        if (i == value.size() || value[i] == ',') {
            parts.push_back(trim(cur));
            cur.clear();
        } else {
            cur += value[i];
        }
    }
    return parts;
}

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool validName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

template <typename T, typename ParseFn>
std::string parseList(const std::string& value, std::vector<T>& out, ParseFn parseOne) {
    std::vector<T> parsed;
    const std::vector<std::string> parts = splitList(value);
    for (size_t i = 0; i < parts.size(); ++i) {
        T v = T();
        if (parts[i].empty() || !parseOne(parts[i], v)) return "invalid value '" + parts[i] + "'";
        parsed.push_back(v);
    }
    out = parsed;
    return "";
}

bool parseInt32(const std::string& s, int32_t& out) {
    int64_t v = 0;
    if (!parseInt64(s, v) || v < INT32_MIN || v > INT32_MAX) return false;
    out = static_cast<int32_t>(v);
    return true;
}

bool parseSeed(const std::string& s, uint64_t& out) {
    int64_t v = 0;
    if (!parseInt64(s, v) || v < 0) return false;
    out = static_cast<uint64_t>(v);
    return true;
}

bool parseBool(const std::string& s, bool& out) {
    if (s == "true" || s == "1" || s == "yes") out = true;
    else if (s == "false" || s == "0" || s == "no") out = false;
    else return false;
    return true;
}

template <typename T>
std::string num(T v) {
    std::ostringstream out;
    out << v;
    return out.str();
}

std::string capacityTag(CapacityMode mode, int32_t fixed) {
    return mode == CapacityMode::Fixed ? "cfixed" + num(fixed) : "c" + toString(mode);
}

bool writeText(const std::string& path, const std::string& text) {
    std::ofstream out(path.c_str());
    out << text;
    return static_cast<bool>(out);
}

}  // namespace

std::string parseSuiteConfig(const std::string& text, SuiteConfig& out) {
    SuiteConfig cfg;
    cfg.raw_text = text;
    std::set<std::string> seen;

    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        std::ostringstream where;
        where << "config line " << lineNo << ": ";
        const size_t eq = line.find('=');
        if (eq == std::string::npos) return where.str() + "expected key = value";
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (value.empty()) return where.str() + "'" + key + "' has no value";
        if (!seen.insert(key).second) return where.str() + "'" + key + "' is set twice";

        std::string err;
        if (key == "name") {
            if (!validName(value)) return where.str() + "name may only use letters, digits, _ - .";
            cfg.name = value;
        } else if (key == "algos") {
            cfg.algos.clear();
            const std::vector<std::string> parts = splitList(value);
            for (size_t i = 0; i < parts.size(); ++i) {
                if (parts[i].empty()) return where.str() + "empty algorithm name";
                cfg.algos.push_back(parts[i]);
            }
        } else if (key == "repeats") {
            if (!parseInt32(value, cfg.repeats) || cfg.repeats < 1) return where.str() + "repeats must be >= 1";
        } else if (key == "out_dir") {
            cfg.out_dir = value;
        } else if (key == "dataset_dir") {
            cfg.dataset_dir = value;
        } else if (key == "save_datasets") {
            if (!parseBool(value, cfg.save_datasets)) return where.str() + "save_datasets must be true or false";
        } else if (key == "overwrite") {
            if (!parseBool(value, cfg.overwrite)) return where.str() + "overwrite must be true or false";
        } else if (key == "agents") {
            err = parseList(value, cfg.agents, parseInt32);
        } else if (key == "resources") {
            err = parseList(value, cfg.resources, parseInt32);
        } else if (key == "density") {
            err = parseList(value, cfg.density, parseDouble);
        } else if (key == "popularity_skew") {
            err = parseList(value, cfg.popularity_skew, parseDouble);
        } else if (key == "group_skew") {
            err = parseList(value, cfg.group_skew, parseDouble);
        } else if (key == "utility_dist") {
            err = parseList(value, cfg.utility_dist, parseUtilityDist);
        } else if (key == "capacity_mode") {
            err = parseList(value, cfg.capacity_mode, parseCapacityMode);
        } else if (key == "capacity") {
            err = parseList(value, cfg.capacity, parseInt32);
        } else if (key == "groups") {
            err = parseList(value, cfg.groups, parseInt32);
        } else if (key == "seed") {
            err = parseList(value, cfg.seeds, parseSeed);
        } else if (key == "utility_min") {
            if (!parseInt32(value, cfg.utility_min)) return where.str() + "utility_min must be an integer";
        } else if (key == "utility_max") {
            if (!parseInt32(value, cfg.utility_max)) return where.str() + "utility_max must be an integer";
        } else {
            return where.str() + "unknown key '" + key + "'";
        }
        if (!err.empty()) return where.str() + key + ": " + err;
    }
    out = cfg;
    return "";
}

std::string loadSuiteConfig(const std::string& path, SuiteConfig& out) {
    std::ifstream in(path.c_str());
    if (!in) return "cannot open config " + path;
    std::ostringstream text;
    text << in.rdbuf();
    const std::string err = parseSuiteConfig(text.str(), out);
    return err.empty() ? "" : path + ": " + err;
}

std::string planSuite(const SuiteConfig& config, std::vector<PlannedDataset>& out) {
    std::vector<PlannedDataset> plan;
    std::set<std::string> ids;

    for (size_t ia = 0; ia < config.agents.size(); ++ia)
    for (size_t ir = 0; ir < config.resources.size(); ++ir)
    for (size_t id = 0; id < config.density.size(); ++id)
    for (size_t ip = 0; ip < config.popularity_skew.size(); ++ip)
    for (size_t iu = 0; iu < config.utility_dist.size(); ++iu)
    for (size_t im = 0; im < config.capacity_mode.size(); ++im) {
        // The `capacity` list only matters for fixed mode; other modes run once.
        const bool fixed = config.capacity_mode[im] == CapacityMode::Fixed;
        const size_t capCount = fixed ? config.capacity.size() : 1;
        for (size_t ic = 0; ic < capCount; ++ic)
        for (size_t ig = 0; ig < config.groups.size(); ++ig)
        for (size_t is = 0; is < config.group_skew.size(); ++is)
        for (size_t ie = 0; ie < config.seeds.size(); ++ie) {
            PlannedDataset p;
            GeneratorConfig& g = p.generator;
            g.agents = config.agents[ia];
            g.resources = config.resources[ir];
            g.density = config.density[id];
            g.popularity_skew = config.popularity_skew[ip];
            g.utility_dist = config.utility_dist[iu];
            g.utility_min = config.utility_min;
            g.utility_max = config.utility_max;
            g.capacity_mode = config.capacity_mode[im];
            g.fixed_capacity = fixed ? config.capacity[ic] : config.capacity[0];
            g.groups = config.groups[ig];
            g.group_skew = config.group_skew[is];
            g.seed = config.seeds[ie];

            p.size_dir = "n" + num(g.agents) + "_m" + num(g.resources);
            const std::string err = validateConfig(g);
            if (!err.empty()) return "invalid parameters for " + p.size_dir + ": " + err;

            p.tag ="d" + num(g.density) + "_ps" + num(g.popularity_skew) + "_u" +
                    toString(g.utility_dist) + "_" + capacityTag(g.capacity_mode, g.fixed_capacity) +
                    "_g" + num(g.groups) + "_gs" + num(g.group_skew) + "_seed" + num(g.seed);
            if (!ids.insert(p.id()).second) {
                return "the sweep contains the same dataset twice (" + p.id() +
                       "); remove repeated values from the lists";
            }
            plan.push_back(p);
        }
    }
    out = plan;
    return "";
}

std::string runSuite(const SuiteConfig& config, SuiteSummary& summary, std::ostream& log) {
    summary = SuiteSummary();

    std::vector<PlannedDataset> plan;
    std::string err = planSuite(config, plan);
    if (!err.empty()) return err;

    std::vector<std::string> algos = config.algos;
    if (algos.empty() || (algos.size() == 1 && algos[0] == "all")) algos = algorithmNames();
    if (algos.empty()) return "no algorithms are registered";
    for (size_t i = 0; i < algos.size(); ++i) {
        if (findAlgorithm(algos[i]) == nullptr) return "unknown algorithm '" + algos[i] + "'";
    }

    const std::string root = config.out_dir + "/" + config.name;
    if (!config.overwrite) {
        for (size_t i = 0; i < plan.size(); ++i) {
            const std::string csv = root + "/" + plan[i].size_dir + "/" + plan[i].tag + "/runs.csv";
            if (fileExists(csv)) {
                return csv + " already exists; raw results are not replaced silently "
                             "(use a new name, delete it, or set overwrite = true)";
            }
        }
    }

    if (!makeDirectories(root)) return "cannot create directory " + root;
    if (!config.raw_text.empty() && !writeText(root + "/config_used.cfg", config.raw_text)) {
        return "cannot write " + root + "/config_used.cfg";
    }

    log << "experiment '" << config.name << "': " << plan.size() << " dataset(s) x " << algos.size()
        << " algorithm(s) x " << config.repeats << " repeat(s)\n";

    for (size_t d = 0; d < plan.size(); ++d) {
        const PlannedDataset& p = plan[d];
        Instance inst;
        err = generateInstance(p.generator, inst);
        if (!err.empty()) return p.id() + ": " + err;

        const std::string dir = root + "/" + p.size_dir + "/" + p.tag;
        if (!makeDirectories(dir)) return "cannot create directory " + dir;
        const std::string csv = dir + "/runs.csv";
        if (config.overwrite) std::remove(csv.c_str());

        std::ostringstream params;
        params << describeConfig(p.generator) << "instance_hash=" << std::hex << std::setfill('0')
               << std::setw(16) << instanceFingerprint(inst) << std::dec << "\nalgorithms=";
        for (size_t i = 0; i < algos.size(); ++i) params << (i ? "," : "") << algos[i];
        params << "\nrepeats=" << config.repeats << "\n";
        if (!writeText(dir + "/params.txt", params.str())) return "cannot write " + dir + "/params.txt";

        if (config.save_datasets) {
            err = writeInstance(config.dataset_dir + "/" + config.name + "/" + p.size_dir + "/" + p.tag,
                                inst, describeConfig(p.generator));
            if (!err.empty()) return err;
        }

        log << "[" << (d + 1) << "/" << plan.size() << "] " << p.id() << " (" << inst.edges.size()
            << " edges)\n";
        ++summary.datasets;

        // Same const Instance for every algorithm and repeat: the comparison is fair by
        // construction, and instance_hash in each row lets anyone verify it afterwards.
        for (size_t a = 0; a < algos.size(); ++a) {
            const AlgorithmFn& fn = *findAlgorithm(algos[a]);
            for (int32_t rep = 1; rep <= config.repeats; ++rep) {
                const RunRecord rec = runOnce(inst, p.id(), algos[a], fn, rep);
                ++summary.runs;
                if (!rec.valid) {
                    ++summary.invalid_runs;
                    log << "  " << algos[a] << ": INVALID - " << rec.validation_error << "\n";
                } else if (rep == 1) {
                    log << "  " << std::left << std::setw(18) << algos[a] << std::right
                        << " assigned=" << rec.result.total_assigned_agents
                        << " utility=" << rec.result.total_utility << " time=" << std::fixed
                        << std::setprecision(3) << rec.result.runtime_milliseconds << " ms\n";
                    log.unsetf(std::ios::floatfield);
                }
                err = appendRecord(csv, rec);
                if (!err.empty()) return err;
            }
        }
    }
    return "";
}

}  // namespace daa
