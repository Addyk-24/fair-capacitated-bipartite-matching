#pragma once

#include <functional>
#include <string>
#include <vector>

#include "common.hpp"

namespace daa {

// Every algorithm stage is exposed to the experiment runner through this one
// signature: take the common Instance, return a MatchingResult. The runner does
// the timing (it overwrites runtime_milliseconds) and the result validation.
using AlgorithmFn = std::function<MatchingResult(const Instance&)>;

// Registers `fn` under `name`. A second registration with the same name
// replaces the first.
void registerAlgorithm(const std::string& name, AlgorithmFn fn);

// Returns nullptr if `name` is not registered.
const AlgorithmFn* findAlgorithm(const std::string& name);

// Registered names in sorted order.
std::vector<std::string> algorithmNames();

// Registers every algorithm that ships in algorithms/. To plug in a new stage,
// declare it in algorithms.hpp and add one line to this function in registry.cpp.
void registerBuiltinAlgorithms();

}  // namespace daa
