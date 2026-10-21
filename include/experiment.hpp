#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "common.hpp"
#include "registry.hpp"

namespace daa {

// One row of the raw results file: a single run of one algorithm on one dataset.
struct RunRecord {
    std::string dataset;
    int32_t repeat = 0;
    int32_t agents = 0;
    int32_t resources = 0;
    size_t edges = 0;
    uint64_t instance_hash = 0;    // instanceFingerprint of the dataset the algorithm received
    bool valid = false;            // every result check passed and the algorithm did not throw
    std::string validation_error;  // empty when valid
    MatchingResult result;         // algorithm_name and runtime_milliseconds always set
};

// --- Hooks --------------------------------------------------------------------------

// Extra result check, run after the built-in validateResult on every run. Return ""
// if the result is acceptable, otherwise a message. Use it for rules that only some
// stages have (for example "fairness constraint satisfied"). A failing check marks
// the run invalid and its message is recorded as "<name>: <message>".
using ResultCheckFn = std::function<std::string(const Instance&, const MatchingResult&)>;
void addResultCheck(const std::string& name, ResultCheckFn fn);
void clearResultChecks();

// Optional custom metric computed from the instance and a valid result. By default,
// the runner records minimum assigned utility / 100. If an algorithm already set
// fairness_score (>= 0) it is left alone. Pass nullptr to remove the metric.
using FairnessMetricFn = std::function<double(const Instance&, const MatchingResult&)>;
void setFairnessMetric(FairnessMetricFn fn);

// --- Running and logging ------------------------------------------------------------

// Runs `fn` once on `inst`, measures wall-clock time around the call only (not
// validation), then validates the output. The measured time replaces whatever
// runtime the algorithm reported so every algorithm is timed the same way.
// An exception thrown by the algorithm is caught and recorded as an invalid run.
RunRecord runOnce(const Instance& inst, const std::string& dataset_name,
                  const std::string& algorithm_name, const AlgorithmFn& fn, int32_t repeat);

// Column names of the raw results file, comma separated, no newline.
std::string resultsHeader();

// Appends one row to `csv_path`, creating the file (and parent directories) and
// writing the header first if needed. Returns "" on success, else an error message.
// fairness_score is written as NA only if it remains unavailable (< 0).
std::string appendRecord(const std::string& csv_path, const RunRecord& record);

}  // namespace daa
