#pragma once

#include <string>

#include "common.hpp"

namespace daa {

// Creates `path` and any missing parents. Returns true if it exists afterwards.
bool makeDirectories(const std::string& path);

// Last path component, ignoring trailing slashes ("data/synthetic/a/" -> "a").
std::string baseName(const std::string& path);

// Dataset on disk = one directory holding three CSV files (see
// docs/specifications/dataset_format.md):
//   agents.csv     agent_id,group_id
//   resources.csv  resource_id,capacity
//   edges.csv      agent_id,resource_id,utility
// plus an optional free-form config.txt describing how it was generated.
//
// writeInstance returns "" on success, otherwise an error message.
// `config_text` (if non-empty) is stored verbatim as config.txt.
std::string writeInstance(const std::string& dir, const Instance& inst,
                          const std::string& config_text = "");

// readInstance also validates the result (validateInstance), so a successful
// return means the instance is safe to hand to any algorithm.
// num_groups is recovered as max(group_id) + 1.
std::string readInstance(const std::string& dir, Instance& out);

}  // namespace daa
