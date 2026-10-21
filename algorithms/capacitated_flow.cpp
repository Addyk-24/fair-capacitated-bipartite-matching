// Capacity-constrained allocation via min-cost flow (Yash, stage "capacitated_flow").
//
// Flow network (nodes: agents 0..A-1, resources A..A+R-1, source S, sink T):
//   S -> agent u         capacity 1,    cost 0
//   agent u -> resource v  capacity 1,  cost depends on the objective (below), eligible edges only
//   resource v -> T      capacity c_v,  cost 0   (resources with c_v = 0 are left out)
// One unit of flow is one assigned agent, so the flow on agent->resource arcs is the
// allocation. Every agent has a single unit-capacity source arc, so it is assigned at
// most once, and the c_v sink arc guarantees resource v never exceeds its capacity.
//
// Two objectives, both solved by successive shortest augmenting paths (SPFA):
//
//   runCapacitatedFlow ("capacitated_flow") - maximum total utility.
//     cost(u,v) = -w_uv. Augment while the cheapest S-T path has negative cost, then stop.
//     Path costs never decrease from one augmentation to the next (cost is convex in
//     the flow value), so stopping at the first non-negative path gives the minimum-cost
//     flow of ANY value. Agents may stay unassigned when that gives more utility. This is
//     the same objective as the weighted stage, so at capacity 1 both must return the
//     same total utility (used as a cross-check in the tests).
//
//   runCapacitatedFlowMaxCardinality ("capacitated_flow_maxcard") - classic min-cost max-flow.
//     cost(u,v) = U_max - w_uv >= 0. Augment until no S-T path exists. Every unit of flow
//     crosses exactly one agent->resource arc, so total cost = F * U_max - total utility
//     with F fixed at the maximum flow. Minimizing cost therefore maximizes utility among
//     all allocations that assign the maximum possible number of agents.
//
// Complexity (both): at most F <= min(A, sum c_v) augmentations. Each SPFA is
//   O(V * E) worst case with V = A + R + 2 and E = |edges| + A + R, so the total is
//   O(F * V * E) worst case. Auxiliary space O(V + E).
// Edge cases verified in tests: empty instance, no edges, zero-capacity resources,
//   isolated agents, capacity 1 (matching), capacity >= A (capacity never binds).
// Determinism: arcs are added in input order and SPFA uses a FIFO queue with strict
//   comparisons, so the same instance always yields the same allocation.
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "algorithms.hpp"

namespace daa {

namespace {

struct Arc {
    size_t to;
    int32_t cap;   // remaining residual capacity
    int64_t cost;
};

class MinCostFlow {
public:
    MinCostFlow(size_t nodes, size_t arcsHint) : adj_(nodes) { arcs_.reserve(2 * arcsHint); }

    // Adds u->v and its residual twin; returns the id of the forward arc.
    // The twin of arc i is always i ^ 1, and its capacity equals the flow on arc i.
    size_t addArc(size_t u, size_t v, int32_t cap, int64_t cost) {
        const size_t id = arcs_.size();
        arcs_.push_back(Arc{v, cap, cost});
        adj_[u].push_back(id);
        arcs_.push_back(Arc{u, 0, -cost});
        adj_[v].push_back(id + 1);
        return id;
    }

    int32_t flowOn(size_t forwardArc) const { return arcs_[forwardArc ^ 1].cap; }

    // Successive shortest paths. Every augmentation pushes exactly one unit because the
    // source arcs have capacity 1. If stopWhenNotImproving, stops once the cheapest path
    // has cost >= 0 (minimum-cost flow of any value); otherwise runs to maximum flow.
    void run(size_t source, size_t sink, bool stopWhenNotImproving) {
        const int64_t INF = std::numeric_limits<int64_t>::max() / 4;
        const size_t n = adj_.size();
        std::vector<int64_t> dist(n);
        std::vector<char> inQueue(n);
        std::vector<size_t> viaArc(n);
        const size_t NONE = std::numeric_limits<size_t>::max();

        while (true) {
            dist.assign(n, INF);
            inQueue.assign(n, 0);
            viaArc.assign(n, NONE);
            std::queue<size_t> q;
            dist[source] = 0;
            q.push(source);
            inQueue[source] = 1;
            while (!q.empty()) {
                const size_t u = q.front();
                q.pop();
                inQueue[u] = 0;
                for (size_t k = 0; k < adj_[u].size(); ++k) {
                    const size_t id = adj_[u][k];
                    const Arc& a = arcs_[id];
                    if (a.cap > 0 && dist[u] + a.cost < dist[a.to]) {
                        dist[a.to] = dist[u] + a.cost;
                        viaArc[a.to] = id;
                        if (!inQueue[a.to]) {
                            inQueue[a.to] = 1;
                            q.push(a.to);
                        }
                    }
                }
            }
            if (dist[sink] >= INF) break;                         // no augmenting path left
            if (stopWhenNotImproving && dist[sink] >= 0) break;   // extra flow would not help

            size_t v = sink;
            while (v != source) {
                const size_t id = viaArc[v];
                arcs_[id].cap -= 1;
                arcs_[id ^ 1].cap += 1;
                v = arcs_[id ^ 1].to;
            }
        }
    }

private:
    std::vector<Arc> arcs_;
    std::vector<std::vector<size_t> > adj_;
};

MatchingResult solve(const Instance& inst, bool maxCardinalityFirst, const char* name) {
    MatchingResult result;
    result.algorithm_name = name;

    const size_t A = static_cast<size_t>(inst.num_agents);
    const size_t R = static_cast<size_t>(inst.num_resources);
    if (A == 0 || R == 0 || inst.edges.empty()) return result;

    // U_max over usable edges (only needed to keep costs non-negative in max-cardinality mode).
    int32_t uMax = std::numeric_limits<int32_t>::min();
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& e = inst.edges[i];
        if (inst.capacity[static_cast<size_t>(e.resource_id)] > 0 && e.utility > uMax) uMax = e.utility;
    }
    if (uMax == std::numeric_limits<int32_t>::min()) return result;  // no usable edge

    const size_t S = A + R;
    const size_t T = S + 1;
    MinCostFlow net(A + R + 2, A + R + inst.edges.size());

    for (size_t a = 0; a < A; ++a) net.addArc(S, a, 1, 0);

    const size_t NONE = std::numeric_limits<size_t>::max();
    std::vector<size_t> arcOfEdge(inst.edges.size(), NONE);
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        const Edge& e = inst.edges[i];
        if (inst.capacity[static_cast<size_t>(e.resource_id)] <= 0) continue;
        const int64_t cost = maxCardinalityFirst
                                 ? static_cast<int64_t>(uMax) - e.utility
                                 : -static_cast<int64_t>(e.utility);
        arcOfEdge[i] = net.addArc(static_cast<size_t>(e.agent_id),
                                  A + static_cast<size_t>(e.resource_id), 1, cost);
    }
    for (size_t r = 0; r < R; ++r) {
        if (inst.capacity[r] > 0) net.addArc(A + r, T, inst.capacity[r], 0);
    }

    net.run(S, T, !maxCardinalityFirst);

    // Read the allocation off the saturated agent->resource arcs, in instance edge order.
    for (size_t i = 0; i < inst.edges.size(); ++i) {
        if (arcOfEdge[i] == NONE || net.flowOn(arcOfEdge[i]) == 0) continue;
        result.assignments.push_back(
            std::make_pair(inst.edges[i].agent_id, inst.edges[i].resource_id));
        result.total_utility += inst.edges[i].utility;
    }
    result.total_assigned_agents = static_cast<int32_t>(result.assignments.size());
    return result;
}

}  // namespace

MatchingResult runCapacitatedFlow(const Instance& inst) {
    return solve(inst, false, "capacitated_flow");
}

MatchingResult runCapacitatedFlowMaxCardinality(const Instance& inst) {
    return solve(inst, true, "capacitated_flow_maxcard");
}

}  // namespace daa
