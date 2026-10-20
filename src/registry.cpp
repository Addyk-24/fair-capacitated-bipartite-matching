#include "registry.hpp"

#include <map>
#include <utility>

#include "algorithms.hpp"

namespace daa {

namespace {

std::map<std::string, AlgorithmFn>& table() {
    static std::map<std::string, AlgorithmFn> algorithms;
    return algorithms;
}

}  // namespace

void registerAlgorithm(const std::string& name, AlgorithmFn fn) { table()[name] = std::move(fn); }

const AlgorithmFn* findAlgorithm(const std::string& name) {
    std::map<std::string, AlgorithmFn>::const_iterator it = table().find(name);
    return it == table().end() ? nullptr : &it->second;
}

std::vector<std::string> algorithmNames() {
    std::vector<std::string> names;
    for (std::map<std::string, AlgorithmFn>::const_iterator it = table().begin(); it != table().end();
         ++it) {
        names.push_back(it->first);
    }
    return names;
}

void registerBuiltinAlgorithms() {
    registerAlgorithm("greedy_reference", runGreedyReference);
    registerAlgorithm("weighted", runWeighted);
    registerAlgorithm("capacitated_flow", runCapacitatedFlow);
    registerAlgorithm("capacitated_flow_maxcard", runCapacitatedFlowMaxCardinality);
    // One line per stage as it lands, e.g.:
    //   registerAlgorithm("cardinality", runCardinality);
}

}  // namespace daa
