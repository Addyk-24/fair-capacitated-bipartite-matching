#include "experiment.hpp"

#include <sys/stat.h>

#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

#include "fingerprint.hpp"
#include "instance_io.hpp"
#include "timer.hpp"
#include "validation.hpp"

namespace daa {

namespace {

struct NamedCheck {
    std::string name;
    ResultCheckFn fn;
};

std::vector<NamedCheck>& checks() {
    static std::vector<NamedCheck> list;
    return list;
}

FairnessMetricFn& fairnessMetric() {
    static FairnessMetricFn metric;
    return metric;
}

bool fileHasContent(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && st.st_size > 0;
}

std::string parentDir(const std::string& path) {
    const size_t cut = path.find_last_of("/\\");
    return cut == std::string::npos ? "" : path.substr(0, cut);
}

std::string hex64(uint64_t v) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << v;
    return out.str();
}

}  // namespace

void addResultCheck(const std::string& name, ResultCheckFn fn) {
    NamedCheck c;
    c.name = name;
    c.fn = std::move(fn);
    checks().push_back(std::move(c));
}

void clearResultChecks() { checks().clear(); }

void setFairnessMetric(FairnessMetricFn fn) { fairnessMetric() = std::move(fn); }

RunRecord runOnce(const Instance& inst, const std::string& dataset_name,
                  const std::string& algorithm_name, const AlgorithmFn& fn, int32_t repeat) {
    RunRecord rec;
    rec.dataset = dataset_name;
    rec.repeat = repeat;
    rec.agents = inst.num_agents;
    rec.resources = inst.num_resources;
    rec.edges = inst.edges.size();
    rec.instance_hash = instanceFingerprint(inst);

    bool threw = false;
    Stopwatch timer;
    try {
        rec.result = fn(inst);
    } catch (const std::exception& e) {
        threw = true;
        rec.validation_error = std::string("algorithm threw: ") + e.what();
    } catch (...) {
        threw = true;
        rec.validation_error = "algorithm threw an unknown exception";
    }
    const double elapsed = timer.elapsedMilliseconds();

    if (threw) rec.result = MatchingResult();
    rec.result.algorithm_name = algorithm_name;
    rec.result.runtime_milliseconds = elapsed;

    if (!threw) rec.validation_error = validateResult(inst, rec.result);
    for (size_t i = 0; !threw && rec.validation_error.empty() && i < checks().size(); ++i) {
        const std::string msg = checks()[i].fn(inst, rec.result);
        if (!msg.empty()) rec.validation_error = checks()[i].name + ": " + msg;
    }
    rec.valid = rec.validation_error.empty();

    if (rec.valid && fairnessMetric() && rec.result.fairness_score < 0.0) {
        rec.result.fairness_score = fairnessMetric()(inst, rec.result);
    }
    return rec;
}

std::string resultsHeader() {
    return "dataset,algorithm,repeat,agents,resources,edges,assigned_count,total_utility,"
           "fairness_score,runtime_ms,valid,instance_hash";
}

std::string appendRecord(const std::string& csv_path, const RunRecord& rec) {
    const std::string parent = parentDir(csv_path);
    if (!parent.empty() && !makeDirectories(parent)) return "cannot create directory " + parent;

    const bool needHeader = !fileHasContent(csv_path);
    std::ofstream out(csv_path.c_str(), std::ios::app);
    if (!out) return "cannot open " + csv_path + " for writing";
    if (needHeader) out << resultsHeader() << '\n';

    out << rec.dataset << ',' << rec.result.algorithm_name << ',' << rec.repeat << ','
        << rec.agents << ',' << rec.resources << ',' << rec.edges << ','
        << rec.result.total_assigned_agents << ',' << rec.result.total_utility << ',';
    if (rec.result.fairness_score < 0.0) out << "NA";
    else out << std::setprecision(10) << rec.result.fairness_score;
    out << ',' << std::fixed << std::setprecision(4) << rec.result.runtime_milliseconds << ','
        << (rec.valid ? 1 : 0) << ',' << hex64(rec.instance_hash) << '\n';
    out.flush();
    return out ? "" : "write to " + csv_path + " failed";
}

}  // namespace daa
