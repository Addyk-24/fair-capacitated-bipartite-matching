#include "validation.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace daa {

namespace {

bool edgeLess(const Edge& e, int32_t agent, int32_t resource) {
    return e.agent_id != agent ? e.agent_id < agent : e.resource_id < resource;
}

}  // namespace

std::string validateInstance(const Instance& inst) {
    std::ostringstream err;
    if (inst.num_agents < 0 || inst.num_resources < 0) return "negative agent or resource count";
    if (inst.num_groups < 1) return "num_groups must be >= 1";
    if (inst.capacity.size() != static_cast<size_t>(inst.num_resources)) {
        return "capacity must have one entry per resource";
    }
    if (inst.group_id.size() != static_cast<size_t>(inst.num_agents)) {
        return "group_id must have one entry per agent";
    }
    for (size_t r = 0; r < inst.capacity.size(); ++r) {
        if (inst.capacity[r] < 0) {
            err << "resource " << r << " has negative capacity";
            return err.str();
        }
    }
    for (size_t a = 0; a < inst.group_id.size(); ++a) {
        if (inst.group_id[a] < 0 || inst.group_id[a] >= inst.num_groups) {
            err << "agent " << a << " has group " << inst.group_id[a] << " outside [0, "
                << inst.num_groups << ")";
            return err.str();
        }
    }
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& e = inst.edges[i];
        if (e.agent_id < 0 || e.agent_id >= inst.num_agents || e.resource_id < 0 ||
            e.resource_id >= inst.num_resources) {
            err << "edge " << i << " (" << e.agent_id << "," << e.resource_id << ") is out of range";
            return err.str();
        }
        if (e.utility < 0 || e.utility > 100) {
            err << "edge " << i << " utility " << e.utility << " is outside [0, 100]";
            return err.str();
        }
        if (i > 0 && !edgeLess(inst.edges[i - 1], e.agent_id, e.resource_id)) {
            err << "edge " << i << " (" << e.agent_id << "," << e.resource_id
                << ") is duplicated or not sorted by (agent_id, resource_id)";
            return err.str();
        }
    }
    return "";
}

std::string validateResult(const Instance& inst, const MatchingResult& result) {
    std::ostringstream err;
    std::vector<char> agentUsed(static_cast<size_t>(inst.num_agents), 0);
    std::vector<int32_t> load(static_cast<size_t>(inst.num_resources), 0);
    int64_t utility = 0;

    for (size_t i = 0; i < result.assignments.size(); ++i) {
        const int32_t a = result.assignments[i].first;
        const int32_t r = result.assignments[i].second;
        if (a < 0 || a >= inst.num_agents || r < 0 || r >= inst.num_resources) {
            err << "assignment (" << a << "," << r << ") is out of range";
            return err.str();
        }
        std::vector<Edge>::const_iterator it =
            std::lower_bound(inst.edges.begin(), inst.edges.end(), std::make_pair(a, r),
                             [](const Edge& e, const std::pair<int32_t, int32_t>& key) {
                                 return edgeLess(e, key.first, key.second);
                             });
        if (it == inst.edges.end() || it->agent_id != a || it->resource_id != r) {
            err << "assignment (" << a << "," << r << ") is not an eligible edge";
            return err.str();
        }
        if (agentUsed[static_cast<size_t>(a)]) {
            err << "agent " << a << " is assigned more than once";
            return err.str();
        }
        agentUsed[static_cast<size_t>(a)] = 1;
        if (++load[static_cast<size_t>(r)] > inst.capacity[static_cast<size_t>(r)]) {
            err << "resource " << r << " exceeds capacity " << inst.capacity[static_cast<size_t>(r)];
            return err.str();
        }
        utility += it->utility;
    }
    if (result.total_assigned_agents != static_cast<int32_t>(result.assignments.size())) {
        err << "total_assigned_agents is " << result.total_assigned_agents << " but there are "
            << result.assignments.size() << " assignments";
        return err.str();
    }
    if (result.total_utility != utility) {
        err << "total_utility is " << result.total_utility << " but the assignments sum to "
            << utility;
        return err.str();
    }
    return "";
}

}  // namespace daa
