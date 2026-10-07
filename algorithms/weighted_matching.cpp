#include "weighted_matching.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

UtilityMatrix buildUtilityMatrix(const WeightedGraph& graph) {
    UtilityMatrix matrix;

    matrix.agents = graph.agents;
    matrix.resources = graph.resources;

    // -1 represents an ineligible agent-resource pair.
    // Valid utilities are expected to be in the range 1-100.
    matrix.values.assign(
        matrix.agents.size(),
        std::vector<int>(matrix.resources.size(), -1)
    );

    for (std::size_t i = 0; i < graph.edges.size(); ++i) {
        const WeightedEdge& edge = graph.edges[i];

        std::vector<std::string>::iterator agent_it =
            std::find(
                matrix.agents.begin(),
                matrix.agents.end(),
                edge.agent_id
            );

        std::vector<std::string>::iterator resource_it =
            std::find(
                matrix.resources.begin(),
                matrix.resources.end(),
                edge.resource_id
            );

        if (agent_it == matrix.agents.end() ||
            resource_it == matrix.resources.end()) {
            continue;
        }

        std::size_t agent_index =
            static_cast<std::size_t>(
                std::distance(matrix.agents.begin(), agent_it)
            );

        std::size_t resource_index =
            static_cast<std::size_t>(
                std::distance(matrix.resources.begin(), resource_it)
            );

        matrix.values[agent_index][resource_index] = edge.utility;
    }

    return matrix;
}


// Hungarian algorithm for minimum-cost assignment.
//
// Input:
//     cost[i][j] = cost of assigning row i to column j
//
// Output:
//     assignment[i] = column assigned to row i
//
// A value of -1 means that the row was not assigned.
static std::vector<int> hungarianMinCost(
    const std::vector<std::vector<long long> >& cost
) {
    int n = static_cast<int>(cost.size());

    if (n == 0) {
        return std::vector<int>();
    }

    int m = static_cast<int>(cost[0].size());

    if (m == 0) {
        return std::vector<int>(n, -1);
    }

    const long long INF =
        std::numeric_limits<long long>::max() / 4;

    std::vector<long long> u(n + 1, 0);
    std::vector<long long> v(m + 1, 0);

    std::vector<int> p(m + 1, 0);
    std::vector<int> way(m + 1, 0);

    for (int i = 1; i <= n; ++i) {
        p[0] = i;

        int j0 = 0;

        std::vector<long long> minv(
            m + 1,
            INF
        );

        std::vector<char> used(
            m + 1,
            false
        );

        do {
            used[j0] = true;

            int i0 = p[j0];

            long long delta = INF;
            int j1 = 0;

            for (int j = 1; j <= m; ++j) {
                if (used[j]) {
                    continue;
                }

                long long current =
                    cost[i0 - 1][j - 1]
                    - u[i0]
                    - v[j];

                if (current < minv[j]) {
                    minv[j] = current;
                    way[j] = j0;
                }

                if (minv[j] < delta) {
                    delta = minv[j];
                    j1 = j;
                }
            }

            for (int j = 0; j <= m; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }

            j0 = j1;

        } while (p[j0] != 0);

        do {
            int j1 = way[j0];

            p[j0] = p[j1];

            j0 = j1;

        } while (j0 != 0);
    }

    std::vector<int> assignment(n, -1);

    for (int j = 1; j <= m; ++j) {
        if (p[j] != 0) {
            assignment[p[j] - 1] = j - 1;
        }
    }

    return assignment;
}


WeightedMatchingResult solveMaximumWeightMatching(
    const WeightedGraph& graph
) {
    WeightedMatchingResult result;

    result.assigned_count = 0;
    result.total_utility = 0;
    result.runtime_ms = 0.0;

    const auto start = std::chrono::high_resolution_clock::now();

    UtilityMatrix matrix = buildUtilityMatrix(graph);

    const std::size_t agent_count = matrix.agents.size();
    const std::size_t resource_count = matrix.resources.size();

    if (agent_count == 0 || resource_count == 0) {
        const auto end = std::chrono::high_resolution_clock::now();

        result.runtime_ms =
            std::chrono::duration<double, std::milli>(
                end - start
            ).count();

        return result;
    }

    /*
     * The Hungarian algorithm below solves a minimum-cost assignment.
     *
     * Our problem is maximum utility.
     *
     * Therefore:
     *
     *     cost = maximum_utility - utility
     *
     * Ineligible edges receive a very large cost so that the
     * algorithm will avoid selecting them whenever possible.
     */

    int maximum_utility = 0;

    for (std::size_t i = 0; i < agent_count; ++i) {
        for (std::size_t j = 0; j < resource_count; ++j) {
            if (matrix.values[i][j] > maximum_utility) {
                maximum_utility = matrix.values[i][j];
            }
        }
    }

    /*
     * The Hungarian implementation requires rows <= columns.
     *
     * If there are more agents than resources, transpose the
     * matrix so that the smaller dimension becomes the rows.
     */
    bool transposed = false;

    std::size_t rows = agent_count;
    std::size_t cols = resource_count;

    if (rows > cols) {
        transposed = true;

        rows = resource_count;
        cols = agent_count;
    }

    const long long FORBIDDEN_COST = 1000000000LL;

    std::vector<std::vector<long long> > cost(
        rows,
        std::vector<long long>(cols, FORBIDDEN_COST)
    );

    if (!transposed) {
        for (std::size_t i = 0; i < agent_count; ++i) {
            for (std::size_t j = 0; j < resource_count; ++j) {

                if (matrix.values[i][j] == -1) {
                    cost[i][j] = FORBIDDEN_COST;
                } else {
                    cost[i][j] =
                        static_cast<long long>(
                            maximum_utility - matrix.values[i][j]
                        );
                }
            }
        }
    } else {
        for (std::size_t i = 0; i < agent_count; ++i) {
            for (std::size_t j = 0; j < resource_count; ++j) {

                if (matrix.values[i][j] == -1) {
                    cost[j][i] = FORBIDDEN_COST;
                } else {
                    cost[j][i] =
                        static_cast<long long>(
                            maximum_utility - matrix.values[i][j]
                        );
                }
            }
        }
    }

    std::vector<int> assignment =
        hungarianMinCost(cost);

    /*
     * Convert the Hungarian result back into agent-resource
     * assignments.
     */
    if (!transposed) {
        for (std::size_t agent_index = 0;
             agent_index < agent_count;
             ++agent_index) {

            int resource_index = assignment[agent_index];

            if (resource_index < 0 ||
                resource_index >= static_cast<int>(resource_count)) {
                continue;
            }

            int utility =
                matrix.values[agent_index]
                            [static_cast<std::size_t>(resource_index)];

            if (utility < 0) {
                continue;
            }

            Assignment selected_assignment;

            selected_assignment.agent_id =
                matrix.agents[agent_index];

            selected_assignment.resource_id =
                matrix.resources[
                    static_cast<std::size_t>(resource_index)
                ];

            selected_assignment.utility = utility;

            result.assignments.push_back(selected_assignment);

            result.assigned_count++;
            result.total_utility += utility;
        }
    } else {
        /*
         * When transposed:
         *
         * Hungarian row    = resource
         * Hungarian column = agent
         */
        for (std::size_t resource_index = 0;
             resource_index < resource_count;
             ++resource_index) {

            int agent_index = assignment[resource_index];

            if (agent_index < 0 ||
                agent_index >= static_cast<int>(agent_count)) {
                continue;
            }

            int utility =
                matrix.values[
                    static_cast<std::size_t>(agent_index)
                ][resource_index];

            if (utility < 0) {
                continue;
            }

            Assignment selected_assignment;

            selected_assignment.agent_id =
                matrix.agents[
                    static_cast<std::size_t>(agent_index)
                ];

            selected_assignment.resource_id =
                matrix.resources[resource_index];

            selected_assignment.utility = utility;

            result.assignments.push_back(selected_assignment);

            result.assigned_count++;
            result.total_utility += utility;
        }
    }

    const auto end = std::chrono::high_resolution_clock::now();

    result.runtime_ms =
        std::chrono::duration<double, std::milli>(
            end - start
        ).count();

    return result;
}