
#include "weighted_matching.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

const long long FORBIDDEN_COST = 1000000000LL;
const long long INF_COST =
    (std::numeric_limits<long long>::max)() / 4;

// Finds the minimum-cost assignment for a rectangular matrix.
// Requirements: number of rows <= number of columns.
// Returns the selected column index for every row.
std::vector<int> hungarianMinCost(
    const std::vector<std::vector<long long> >& cost
) {
    const int n = static_cast<int>(cost.size());

    if (n == 0) {
        return std::vector<int>();
    }

    const int m = static_cast<int>(cost[0].size());

    if (m < n) {
        throw std::invalid_argument(
            "Hungarian algorithm requires rows <= columns."
        );
    }

    for (int i = 0; i < n; ++i) {
        if (static_cast<int>(cost[i].size()) != m) {
            throw std::invalid_argument(
                "Cost matrix must be rectangular."
            );
        }
    }

    std::vector<long long> u(n + 1, 0);
    std::vector<long long> v(m + 1, 0);
    std::vector<int> p(m + 1, 0);
    std::vector<int> way(m + 1, 0);

    for (int i = 1; i <= n; ++i) {
        p[0] = i;

        int j0 = 0;

        std::vector<long long> minv(m + 1, INF_COST);
        std::vector<bool> used(m + 1, false);

        do {
            used[j0] = true;

            const int i0 = p[j0];

            long long delta = INF_COST;
            int j1 = 0;

            for (int j = 1; j <= m; ++j) {
                if (!used[j]) {
                    const long long current =
                        cost[i0 - 1][j - 1] - u[i0] - v[j];

                    if (current < minv[j]) {
                        minv[j] = current;
                        way[j] = j0;
                    }

                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
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
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }

    std::vector<int> rowToColumn(n, -1);

    for (int j = 1; j <= m; ++j) {
        if (p[j] != 0) {
            rowToColumn[p[j] - 1] = j - 1;
        }
    }

    return rowToColumn;
}

} // namespace

UtilityMatrix buildUtilityMatrix(const WeightedGraph& graph) {
    UtilityMatrix matrix;
    matrix.agents = graph.agents;
    matrix.resources = graph.resources;

    const int A = static_cast<int>(graph.agents.size());
    const int R = static_cast<int>(graph.resources.size());

    matrix.values.assign(
        A,
        std::vector<int>(R, -1)
    );

    for (std::size_t e = 0; e < graph.edges.size(); ++e) {
        const WeightedEdge& edge = graph.edges[e];

        if (edge.utility < 1 || edge.utility > 100) {
            throw std::invalid_argument(
                "Utility must be between 1 and 100."
            );
        }

        int agentIndex = -1;
        int resourceIndex = -1;

        for (int i = 0; i < A; ++i) {
            if (graph.agents[i] == edge.agent_id) {
                agentIndex = i;
                break;
            }
        }

        for (int j = 0; j < R; ++j) {
            if (graph.resources[j] == edge.resource_id) {
                resourceIndex = j;
                break;
            }
        }

        if (agentIndex == -1 || resourceIndex == -1) {
            throw std::invalid_argument(
                "Edge references an unknown agent or resource."
            );
        }

        // -1 means the pair has not been encountered yet.
        // Therefore, any other value indicates a duplicate edge.
        if (matrix.values[agentIndex][resourceIndex] != -1) {
            throw std::invalid_argument(
                "Duplicate edge for the same agent-resource pair."
            );
        }

        matrix.values[agentIndex][resourceIndex] = edge.utility;
    }

    return matrix;
}

WeightedMatchingResult solveMaximumWeightMatching(
    const WeightedGraph& graph
) {
    using Clock = std::chrono::high_resolution_clock;

    const Clock::time_point start = Clock::now();

    WeightedMatchingResult result;
    result.assigned_count = 0;
    result.total_utility = 0;
    result.runtime_ms = 0.0;

    UtilityMatrix matrix = buildUtilityMatrix(graph);

    const int A = static_cast<int>(matrix.agents.size());
    const int R = static_cast<int>(matrix.resources.size());

    if (A == 0 || R == 0) {
        const Clock::time_point finish = Clock::now();

        result.runtime_ms =
            std::chrono::duration<double, std::milli>(
                finish - start
            ).count();

        return result;
    }

    // The smaller side becomes the row set.
    // This keeps the Hungarian problem rectangular and efficient.
    const bool agentsAreRows = (A <= R);

    const int rowCount = std::min(A, R);
    const int realColumnCount = std::max(A, R);

    // One dummy column per row allows every row to remain unmatched.
    const int columnCount = realColumnCount + rowCount;

    int maximumUtility = 0;

    for (int i = 0; i < A; ++i) {
        for (int j = 0; j < R; ++j) {
            if (matrix.values[i][j] >= 1) {
                maximumUtility = std::max(
                    maximumUtility,
                    matrix.values[i][j]
                );
            }
        }
    }

    // For each row, assigning a real eligible edge costs
    // maximumUtility - utility.
    // Assigning a dummy column costs maximumUtility.
    //
    // Every row receives exactly one real or dummy column.
    // Therefore, minimizing total cost maximizes total utility.
    // Ineligible real edges receive a prohibitive cost.
    std::vector<std::vector<long long> > cost(
        rowCount,
        std::vector<long long>(
            columnCount,
            static_cast<long long>(maximumUtility)
        )
    );

    for (int row = 0; row < rowCount; ++row) {
        for (int column = 0; column < realColumnCount; ++column) {
            int utility = -1;

            if (agentsAreRows) {
                // Rows are agents; real columns are resources.
                utility = matrix.values[row][column];
            } else {
                // Rows are resources; real columns are agents.
                utility = matrix.values[column][row];
            }

            if (utility >= 1) {
                cost[row][column] =
                    maximumUtility - utility;
            } else {
                cost[row][column] = FORBIDDEN_COST;
            }
        }
    }

    const std::vector<int> rowToColumn =
        hungarianMinCost(cost);

    for (int row = 0; row < rowCount; ++row) {
        const int column = rowToColumn[row];

        // A dummy column represents an unmatched row.
        if (column < 0 || column >= realColumnCount) {
            continue;
        }

        int agentIndex;
        int resourceIndex;

        if (agentsAreRows) {
            agentIndex = row;
            resourceIndex = column;
        } else {
            agentIndex = column;
            resourceIndex = row;
        }

        const int utility =
            matrix.values[agentIndex][resourceIndex];

        // Never return an ineligible pair, even if the solver's
        // output were unexpectedly to contain one.
        if (utility < 1) {
            continue;
        }

        Assignment assignment;
        assignment.agent_id = matrix.agents[agentIndex];
        assignment.resource_id = matrix.resources[resourceIndex];
        assignment.utility = utility;

        result.assignments.push_back(assignment);
        result.total_utility += utility;
        ++result.assigned_count;
    }

    const Clock::time_point finish = Clock::now();

    result.runtime_ms =
        std::chrono::duration<double, std::milli>(
            finish - start
        ).count();

    return result;
}
