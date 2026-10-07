#include "../../algorithms/weighted_matching.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void addAgent(WeightedGraph& graph, const std::string& agent) {
    if (std::find(
            graph.agents.begin(),
            graph.agents.end(),
            agent
        ) == graph.agents.end()) {

        graph.agents.push_back(agent);
    }
}

void addResource(
    WeightedGraph& graph,
    const std::string& resource
) {
    if (std::find(
            graph.resources.begin(),
            graph.resources.end(),
            resource
        ) == graph.resources.end()) {

        graph.resources.push_back(resource);
    }
}

void addEdge(
    WeightedGraph& graph,
    const std::string& agent,
    const std::string& resource,
    int utility
) {
    addAgent(graph, agent);
    addResource(graph, resource);

    WeightedEdge edge;

    edge.agent_id = agent;
    edge.resource_id = resource;
    edge.utility = utility;

    graph.edges.push_back(edge);
}


/*
 * Test 1:
 * Normal 3x3 weighted matching.
 *
 * Expected:
 * A1 -> R1 = 10
 * A2 -> R2 = 9
 * A3 -> R3 = 10
 *
 * Total utility = 29
 */
void testNormalMatching() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A1", "R2", 6);
    addEdge(graph, "A1", "R3", 2);

    addEdge(graph, "A2", "R1", 8);
    addEdge(graph, "A2", "R2", 9);
    addEdge(graph, "A2", "R3", 4);

    addEdge(graph, "A3", "R1", 5);
    addEdge(graph, "A3", "R2", 7);
    addEdge(graph, "A3", "R3", 10);

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 3);
    assert(result.total_utility == 29);

    std::cout << "Test 1 passed: Normal 3x3 matching\n";
}


/*
 * Test 2:
 * One agent has no eligible resource.
 *
 * A3 has no edges.
 *
 * Expected:
 * A1 -> R1 = 10
 * A2 -> R2 = 9
 *
 * A3 remains unassigned.
 */
void testAgentWithNoEligibleResource() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A1", "R2", 5);

    addEdge(graph, "A2", "R1", 4);
    addEdge(graph, "A2", "R2", 9);

    addAgent(graph, "A3");

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 2);
    assert(result.total_utility == 19);

    std::cout
        << "Test 2 passed: Agent with no eligible resource\n";
}


/*
 * Test 3:
 * More agents than resources.
 *
 * 4 agents, 2 resources.
 *
 * Only two agents can be assigned.
 */
void testMoreAgentsThanResources() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A1", "R2", 2);

    addEdge(graph, "A2", "R1", 8);
    addEdge(graph, "A2", "R2", 9);

    addEdge(graph, "A3", "R1", 7);
    addEdge(graph, "A3", "R2", 6);

    addEdge(graph, "A4", "R1", 5);
    addEdge(graph, "A4", "R2", 4);

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 2);
    assert(result.total_utility == 19);

    std::cout
        << "Test 3 passed: More agents than resources\n";
}


/*
 * Test 4:
 * Missing/ineligible edges.
 *
 * A1 is eligible only for R1.
 * A2 is eligible only for R2.
 *
 * A1-R2 and A2-R1 are absent.
 */
void testIneligibleEdges() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A2", "R2", 8);

    // Add resources explicitly so that the missing edges
    // are represented in the matrix as -1.
    addResource(graph, "R1");
    addResource(graph, "R2");

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 2);
    assert(result.total_utility == 18);

    std::cout
        << "Test 4 passed: Ineligible edges\n";
}


/*
 * Test 5:
 * Utility trade-off.
 *
 * A greedy choice would be:
 *
 * A1 -> R1 = 10
 * A2 -> R2 = 8
 *
 * Total = 18
 *
 * But the optimal solution is:
 *
 * A1 -> R2 = 9
 * A2 -> R1 = 9
 *
 * Total = 18
 *
 * This test verifies the algorithm still finds
 * the maximum total utility.
 */
void testUtilityTradeoff() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A1", "R2", 9);

    addEdge(graph, "A2", "R1", 9);
    addEdge(graph, "A2", "R2", 1);

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    /*
     * Greedy choice:
     *
     * A1 -> R1 = 10
     * A2 -> R2 = 1
     *
     * Total = 11
     *
     * Optimal choice:
     *
     * A1 -> R2 = 9
     * A2 -> R1 = 9
     *
     * Total = 18
     */

    assert(result.assigned_count == 2);
    assert(result.total_utility == 18);

    std::cout
        << "Test 5 passed: Utility trade-off\n";
}

int main() {
    std::cout
        << "Running maximum-weight matching tests...\n\n";

    testNormalMatching();
    testAgentWithNoEligibleResource();
    testMoreAgentsThanResources();
    testIneligibleEdges();
    testUtilityTradeoff();

    std::cout
        << "\nAll 5 maximum-weight matching tests passed!\n";

    return 0;
}