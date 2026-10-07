
#include "../../algorithms/weighted_matching.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    WeightedGraph graph;

    std::ifstream file("data/weighted/small_weighted.csv");

    if (!file.is_open()) {
        std::cerr << "Error: Cannot open CSV file.\n";
        return 1;
    }

    std::string line;
    std::getline(file, line); // Skip CSV header

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string agent, resource, utilityText, group;

        std::getline(ss, agent, ',');
        std::getline(ss, resource, ',');
        std::getline(ss, utilityText, ',');
        std::getline(ss, group, ',');

        int utility = std::stoi(utilityText);

        if (std::find(graph.agents.begin(), graph.agents.end(), agent)
            == graph.agents.end()) {
            graph.agents.push_back(agent);
        }

        if (std::find(graph.resources.begin(), graph.resources.end(), resource)
            == graph.resources.end()) {
            graph.resources.push_back(resource);
        }

        graph.edges.push_back({agent, resource, utility});
    }

    UtilityMatrix matrix = buildUtilityMatrix(graph);

    assert(matrix.agents.size() == 3);
    assert(matrix.resources.size() == 3);

    const int expected[3][3] = {
        {10, 6, 2},
        {8, 9, 4},
        {5, 7, 10}
    };

    std::cout << "Generated Utility Matrix:\n\n";
    std::cout << "       R1  R2  R3\n";

    for (std::size_t i = 0; i < matrix.agents.size(); ++i) {
        std::cout << matrix.agents[i] << "     ";

        for (std::size_t j = 0; j < matrix.resources.size(); ++j) {
            assert(matrix.values[i][j] == expected[i][j]);
            std::cout << matrix.values[i][j] << "   ";
        }

        std::cout << "\n";
    }

    std::cout << "\nAll utility matrix tests passed!\n";

    return 0;
}
