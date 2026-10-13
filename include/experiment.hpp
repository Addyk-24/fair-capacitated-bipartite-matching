#pragma once

#include <cstddef>
#include <cstdint>
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
    bool valid = false;           // validateResult passed
    std::string validation_error; // empty when valid
    MatchingResult result;        // algorithm_name and runtime_milliseconds always set
};

// Runs `fn` once on `inst`, measures wall-clock time around the call only (not
// validation), then validates the output. The measured time replaces whatever
// runtime the algorithm reported so every algorithm is timed the same way.
RunRecord runOnce(const Instance& inst, const std::string& dataset_name,
                  const std::string& algorithm_name, const AlgorithmFn& fn, int32_t repeat);

// Column names of the raw results file, comma separated, no newline.
std::string resultsHeader();

// Appends one row to `csv_path`, creating the file (and parent directories) and
// writing the header first if needed. Returns "" on success, else an error message.
// fairness_score is written as NA until a fairness metric is available.
std::string appendRecord(const std::string& csv_path, const RunRecord& record);

}  // namespace daa
