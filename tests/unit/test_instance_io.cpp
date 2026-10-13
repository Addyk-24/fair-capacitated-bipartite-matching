#include <cstdio>
#include <fstream>
#include <string>

#include "generator.hpp"
#include "instance_io.hpp"
#include "test_support.hpp"

namespace {

using namespace daa;

const std::string kDir = "test_tmp_instance_io";

void writeFile(const std::string& name, const std::string& text) {
    std::ofstream out((kDir + "/" + name).c_str());
    out << text;
}

void cleanup() {
    const char* files[] = {"agents.csv", "resources.csv", "edges.csv", "config.txt"};
    for (size_t i = 0; i < 4; ++i) std::remove((kDir + "/" + files[i]).c_str());
}

void writeValid() {
    writeFile("agents.csv", "agent_id,group_id\n0,0\n1,1\n2,0\n");
    writeFile("resources.csv", "resource_id,capacity\n0,2\n1,0\n");
    writeFile("edges.csv", "agent_id,resource_id,utility\n0,0,5\n0,1,3\n2,0,9\n");
}

void testRoundTrip() {
    GeneratorConfig cfg;
    cfg.agents = 40;
    cfg.resources = 7;
    cfg.density = 0.5;
    cfg.groups = 3;
    Instance original;
    CHECK_EQ(generateInstance(cfg, original), std::string(""));

    CHECK_EQ(writeInstance(kDir, original, "seed=42\n"), std::string(""));
    Instance loaded;
    CHECK_EQ(readInstance(kDir, loaded), std::string(""));

    CHECK_EQ(loaded.num_agents, original.num_agents);
    CHECK_EQ(loaded.num_resources, original.num_resources);
    CHECK_EQ(loaded.num_groups, original.num_groups);
    CHECK(loaded.capacity == original.capacity);
    CHECK(loaded.group_id == original.group_id);
    CHECK_EQ(loaded.edges.size(), original.edges.size());
    bool same = true;
    for (size_t i = 0; i < loaded.edges.size(); ++i) {
        same = same && loaded.edges[i].agent_id == original.edges[i].agent_id &&
               loaded.edges[i].resource_id == original.edges[i].resource_id &&
               loaded.edges[i].utility == original.edges[i].utility;
    }
    CHECK(same);

    std::ifstream meta((kDir + "/config.txt").c_str());
    std::string line;
    std::getline(meta, line);
    CHECK_EQ(line, std::string("seed=42"));
}

void testHandWrittenAndIsolatedAgents() {
    writeValid();
    Instance inst;
    CHECK_EQ(readInstance(kDir, inst), std::string(""));
    CHECK_EQ(inst.num_agents, 3);      // agent 1 has no edges but still exists
    CHECK_EQ(inst.num_resources, 2);
    CHECK_EQ(inst.num_groups, 2);
    CHECK_EQ(inst.capacity[1], 0);     // zero-capacity resource survives
    CHECK_EQ(inst.edges.size(), static_cast<size_t>(3));
    CHECK_EQ(inst.edges[2].utility, 9);
}

void testToleratesWindowsLineEndingsAndBlankLines() {
    writeFile("agents.csv", "agent_id,group_id\r\n0,0\r\n\r\n1,0\r\n");
    writeFile("resources.csv", "resource_id,capacity\r\n0,1\r\n");
    writeFile("edges.csv", "agent_id,resource_id,utility\r\n0,0,4\r\n1,0,6\r\n");
    Instance inst;
    CHECK_EQ(readInstance(kDir, inst), std::string(""));
    CHECK_EQ(inst.num_agents, 2);
    CHECK_EQ(inst.edges.size(), static_cast<size_t>(2));
}

void testRejectsMalformedInput() {
    Instance inst;

    writeValid();
    writeFile("agents.csv", "id,group\n0,0\n");
    CHECK(!readInstance(kDir, inst).empty());  // wrong header

    writeValid();
    writeFile("agents.csv", "agent_id,group_id\n0,0\n2,0\n");
    CHECK(!readInstance(kDir, inst).empty());  // id gap

    writeValid();
    writeFile("agents.csv", "agent_id,group_id\n0,x\n");
    CHECK(!readInstance(kDir, inst).empty());  // not a number

    writeValid();
    writeFile("edges.csv", "agent_id,resource_id,utility\n0,0\n");
    CHECK(!readInstance(kDir, inst).empty());  // too few columns

    writeValid();
    writeFile("edges.csv", "agent_id,resource_id,utility\n0,0,5\n0,0,6\n");
    CHECK(!readInstance(kDir, inst).empty());  // duplicate pair

    writeValid();
    writeFile("edges.csv", "agent_id,resource_id,utility\n0,9,5\n");
    CHECK(!readInstance(kDir, inst).empty());  // resource out of range

    writeValid();
    writeFile("resources.csv", "resource_id,capacity\n0,-3\n");
    CHECK(!readInstance(kDir, inst).empty());  // negative capacity

    writeValid();
    std::remove((kDir + "/edges.csv").c_str());
    CHECK(!readInstance(kDir, inst).empty());  // missing file

    CHECK(!readInstance("no_such_dataset_dir", inst).empty());
}

void testWriteRefusesInvalidInstance() {
    Instance bad;
    bad.num_agents = 1;  // group_id missing
    CHECK(!writeInstance(kDir, bad).empty());
}

void testPathHelpers() {
    CHECK_EQ(baseName("data/synthetic/small_01"), std::string("small_01"));
    CHECK_EQ(baseName("data/synthetic/small_01/"), std::string("small_01"));
    CHECK_EQ(baseName("data\\synthetic\\small_01"), std::string("small_01"));
    CHECK_EQ(baseName("plain"), std::string("plain"));
    CHECK(makeDirectories(kDir + "/nested/deeper"));
    CHECK(makeDirectories(kDir + "/nested/deeper"));  // already exists is fine
}

}  // namespace

int main() {
    CHECK(makeDirectories(kDir));
    testRoundTrip();
    testHandWrittenAndIsolatedAgents();
    testToleratesWindowsLineEndingsAndBlankLines();
    testRejectsMalformedInput();
    testWriteRefusesInvalidInstance();
    testPathHelpers();
    cleanup();
    return finishTests("test_instance_io");
}
