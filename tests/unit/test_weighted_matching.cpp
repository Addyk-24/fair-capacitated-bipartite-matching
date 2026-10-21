
#include "../../algorithms/weighted_matching.hpp"

#include <cassert>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

void addAgent(WeightedGraph& graph, const std::string& id) {
    for (std::size_t i = 0; i < graph.agents.size(); ++i) {
        if (graph.agents[i] == id) {
            return;
        }
    }

    graph.agents.push_back(id);
}

void addResource(WeightedGraph& graph, const std::string& id) {
    for (std::size_t i = 0; i < graph.resources.size(); ++i) {
        if (graph.resources[i] == id) {
            return;
        }
    }

    graph.resources.push_back(id);
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

void verifyMatching(
    const WeightedGraph& graph,
    const WeightedMatchingResult& result
) {
    std::set<std::string> assignedAgents;
    std::set<std::string> assignedResources;
    long long calculatedUtility = 0;

    assert(result.assigned_count ==
           static_cast<int>(result.assignments.size()));

    for (std::size_t i = 0; i < result.assignments.size(); ++i) {
        const Assignment& assignment = result.assignments[i];

        // No agent may be assigned more than once.
        assert(assignedAgents.insert(assignment.agent_id).second);

        // No resource may be assigned more than once.
        assert(assignedResources.insert(assignment.resource_id).second);

        // Every returned assignment must correspond to an eligible edge.
        bool eligible = false;

        for (std::size_t e = 0; e < graph.edges.size(); ++e) {
            const WeightedEdge& edge = graph.edges[e];

            if (edge.agent_id == assignment.agent_id &&
                edge.resource_id == assignment.resource_id) {
                eligible = true;
                assert(edge.utility == assignment.utility);
                break;
            }
        }

        assert(eligible);
        calculatedUtility += assignment.utility;
    }

    assert(calculatedUtility == result.total_utility);
}

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

    verifyMatching(graph, result);

    std::cout << "Test 1 passed: Normal 3x3 matching\n";
}

void testAgentWithNoEligibleResource() {
    WeightedGraph graph;

    addAgent(graph, "A1");
    addAgent(graph, "A2");
    addAgent(graph, "A3");

    addResource(graph, "R1");
    addResource(graph, "R2");

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A2", "R2", 9);

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 2);
    assert(result.total_utility == 19);

    verifyMatching(graph, result);

    std::cout << "Test 2 passed: Agent with no eligible resource\n";
}

void testMoreAgentsThanResources() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A1", "R2", 4);
    addEdge(graph, "A2", "R1", 9);
    addEdge(graph, "A2", "R2", 8);
    addEdge(graph, "A3", "R1", 7);
    addEdge(graph, "A3", "R2", 3);
    addAgent(graph, "A4");

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 2);
    assert(result.total_utility == 18);

    verifyMatching(graph, result);

    std::cout << "Test 3 passed: More agents than resources\n";
}

void testIneligibleEdges() {
    WeightedGraph graph;

    addAgent(graph, "A1");
    addAgent(graph, "A2");
    addResource(graph, "R1");
    addResource(graph, "R2");

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A2", "R2", 8);

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 2);
    assert(result.total_utility == 18);

    verifyMatching(graph, result);

    std::cout << "Test 4 passed: Ineligible edges\n";
}

void testUtilityTradeoff() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A1", "R2", 9);
    addEdge(graph, "A2", "R1", 9);
    addEdge(graph, "A2", "R2", 1);

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    // Greedy assignment: A1->R1 (10), A2->R2 (1), total 11.
    // Optimal assignment: A1->R2 (9), A2->R1 (9), total 18.
    assert(result.assigned_count == 2);
    assert(result.total_utility == 18);

    verifyMatching(graph, result);

    std::cout << "Test 5 passed: Utility trade-off\n";
}

void testMaximumUtilityCanLeaveAgentUnmatched() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 100);
    addEdge(graph, "A1", "R2", 1);
    addEdge(graph, "A2", "R1", 1);

    // A2 -> R2 is ineligible because no edge exists.

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    // The optimal solution is A1->R1 = 100.
    // A2 remains unmatched. Assigning both agents would give utility 2.
    assert(result.assigned_count == 1);
    assert(result.total_utility == 100);
    assert(result.assignments[0].agent_id == "A1");
    assert(result.assignments[0].resource_id == "R1");

    verifyMatching(graph, result);

    std::cout
        << "Test 6 passed: Maximum utility permits unmatched agents\n";
}

void testMoreResourcesThanAgents() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 5);
    addEdge(graph, "A1", "R2", 10);
    addEdge(graph, "A1", "R3", 3);

    addEdge(graph, "A2", "R1", 8);
    addEdge(graph, "A2", "R2", 4);
    addEdge(graph, "A2", "R3", 9);

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 2);
    assert(result.total_utility == 19);

    verifyMatching(graph, result);

    std::cout << "Test 7 passed: More resources than agents\n";
}

void testEmptyGraph() {
    WeightedGraph graph;

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 0);
    assert(result.total_utility == 0);
    assert(result.assignments.empty());

    verifyMatching(graph, result);

    std::cout << "Test 8 passed: Empty graph\n";
}

void testNoEligibleEdges() {
    WeightedGraph graph;

    addAgent(graph, "A1");
    addAgent(graph, "A2");
    addResource(graph, "R1");
    addResource(graph, "R2");

    WeightedMatchingResult result =
        solveMaximumWeightMatching(graph);

    assert(result.assigned_count == 0);
    assert(result.total_utility == 0);
    assert(result.assignments.empty());

    verifyMatching(graph, result);

    std::cout << "Test 9 passed: No eligible edges\n";
}

void testZeroUtilityIsStillEligible() {
    WeightedGraph graph;
    addEdge(graph, "A1", "R1", 0);
    const UtilityMatrix matrix = buildUtilityMatrix(graph);
    assert(matrix.values[0][0] == 0);
    WeightedMatchingResult result = solveMaximumWeightMatching(graph);
    // The weighted objective permits either empty or zero-utility allocations.
    // The solver must never treat the edge as an invalid matrix sentinel.
    verifyMatching(graph, result);
    assert(result.total_utility == 0);
    assert(result.assigned_count <= 1);
    std::cout << "Test 10 passed: Zero utility edge remains eligible\n";
}

void testInvalidUtilityRejected() {
    WeightedGraph graph;
    addEdge(graph, "A1", "R1", 101);

    bool rejected = false;

    try {
        buildUtilityMatrix(graph);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    std::cout << "Test 10 passed: Invalid utility rejected\n";
}

void testDuplicateEdgeRejected() {
    WeightedGraph graph;

    addEdge(graph, "A1", "R1", 10);
    addEdge(graph, "A1", "R1", 20);

    bool rejected = false;

    try {
        buildUtilityMatrix(graph);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    std::cout << "Test 11 passed: Duplicate edge rejected\n";
}

int main() {
    std::cout << "Running maximum-weight matching tests...\n\n";

    testNormalMatching();
    testAgentWithNoEligibleResource();
    testMoreAgentsThanResources();
    testIneligibleEdges();
    testUtilityTradeoff();
    testMaximumUtilityCanLeaveAgentUnmatched();
    testMoreResourcesThanAgents();
    testEmptyGraph();
    testNoEligibleEdges();
    testZeroUtilityIsStillEligible();
    testInvalidUtilityRejected();
    testDuplicateEdgeRejected();

    std::cout
        << "\nAll 11 maximum-weight matching tests passed!\n";

    return 0;
}
