
#include "../../algorithms/weighted_matching.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

WeightedGraph loadCSV(const std::string& path) {
    std::ifstream file(path.c_str());

    if (!file.is_open()) {
        throw std::runtime_error("Could not open CSV: " + path);
    }

    WeightedGraph graph;
    std::string line;

    // Skip the CSV header.
    if (!std::getline(file, line)) {
        throw std::runtime_error("CSV is empty.");
    }

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream row(line);

        std::string agent;
        std::string resource;
        std::string utilityText;
        std::string group;

        if (!std::getline(row, agent, ',') ||
            !std::getline(row, resource, ',') ||
            !std::getline(row, utilityText, ',')) {
            throw std::runtime_error("Malformed CSV row.");
        }

        std::getline(row, group, ',');

        std::stringstream utilityStream(utilityText);
        int utility;

        if (!(utilityStream >> utility)) {
            throw std::runtime_error("Invalid utility in CSV.");
        }

        bool agentExists = false;
        bool resourceExists = false;

        for (std::size_t i = 0; i < graph.agents.size(); ++i) {
            if (graph.agents[i] == agent) {
                agentExists = true;
                break;
            }
        }

        for (std::size_t i = 0; i < graph.resources.size(); ++i) {
            if (graph.resources[i] == resource) {
                resourceExists = true;
                break;
            }
        }

        if (!agentExists) {
            graph.agents.push_back(agent);
        }

        if (!resourceExists) {
            graph.resources.push_back(resource);
        }

        WeightedEdge edge;
        edge.agent_id = agent;
        edge.resource_id = resource;
        edge.utility = utility;

        graph.edges.push_back(edge);
    }

    return graph;
}

int main() {
    const WeightedGraph graph =
        loadCSV("data/weighted/small_weighted.csv");

    const UtilityMatrix matrix = buildUtilityMatrix(graph);

    assert(matrix.agents.size() == 3);
    assert(matrix.resources.size() == 3);
    assert(matrix.values.size() == 3);

    const int expected[3][3] = {
        {10, 6, 2},
        {8, 9, 4},
        {5, 7, 10}
    };

    std::cout << "Generated Utility Matrix:\n\n";
    std::cout << "       R1  R2  R3\n";

    for (std::size_t i = 0; i < matrix.agents.size(); ++i) {
        assert(matrix.values[i].size() == 3);

        std::cout << matrix.agents[i] << "     ";

        for (std::size_t j = 0; j < matrix.resources.size(); ++j) {
            assert(matrix.values[i][j] == expected[i][j]);
            std::cout << matrix.values[i][j] << "   ";
        }

        std::cout << '\n';
    }

    std::cout << "\nAll utility matrix tests passed!\n";

    return 0;
}
