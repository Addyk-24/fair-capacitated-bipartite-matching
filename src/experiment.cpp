#include "experiment.hpp"

#include <sys/stat.h>

#include <fstream>
#include <iomanip>

#include "instance_io.hpp"
#include "timer.hpp"
#include "validation.hpp"

namespace daa {

namespace {

bool fileHasContent(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && st.st_size > 0;
}

std::string parentDir(const std::string& path) {
    const size_t cut = path.find_last_of("/\\");
    return cut == std::string::npos ? "" : path.substr(0, cut);
}

}  // namespace

RunRecord runOnce(const Instance& inst, const std::string& dataset_name,
                  const std::string& algorithm_name, const AlgorithmFn& fn, int32_t repeat) {
    RunRecord rec;
    rec.dataset = dataset_name;
    rec.repeat = repeat;
    rec.agents = inst.num_agents;
    rec.resources = inst.num_resources;
    rec.edges = inst.edges.size();

    Stopwatch timer;
    rec.result = fn(inst);
    const double elapsed = timer.elapsedMilliseconds();

    rec.result.algorithm_name = algorithm_name;
    rec.result.runtime_milliseconds = elapsed;
    rec.validation_error = validateResult(inst, rec.result);
    rec.valid = rec.validation_error.empty();
    return rec;
}

std::string resultsHeader() {
    return "dataset,algorithm,repeat,agents,resources,edges,assigned_count,total_utility,"
           "fairness_score,runtime_ms,valid";
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
        << (rec.valid ? 1 : 0) << '\n';
    out.flush();
    return out ? "" : "write to " + csv_path + " failed";
}

}  // namespace daa
