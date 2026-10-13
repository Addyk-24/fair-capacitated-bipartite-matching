#include "instance_io.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

#include "cli.hpp"
#include "validation.hpp"

namespace daa {

namespace {

bool isSeparator(char c) { return c == '/' || c == '\\'; }

bool directoryExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

void makeOne(const std::string& path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0777);
#endif
}

std::string joinPath(const std::string& dir, const std::string& file) {
    if (dir.empty() || isSeparator(dir[dir.size() - 1])) return dir + file;
    return dir + "/" + file;
}

std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ',') {
            fields.push_back(cur);
            cur.clear();
        } else if (line[i] != '\r' && line[i] != ' ' && line[i] != '\t') {
            cur += line[i];
        }
    }
    fields.push_back(cur);
    return fields;
}

// Reads a CSV file with a fixed header; calls onRow(fields, lineNumber) for each
// data row and stops at the first non-empty error string.
template <typename RowFn>
std::string readCsv(const std::string& path, const std::string& header, size_t columns,
                    RowFn onRow) {
    std::ifstream in(path.c_str());
    if (!in) return "cannot open " + path;

    std::string line;
    if (!std::getline(in, line) || splitCsv(line) != splitCsv(header)) {
        return path + ": expected header '" + header + "'";
    }
    int lineNo = 1;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::vector<std::string> f = splitCsv(line);
        if (f.size() == 1 && f[0].empty()) continue;  // blank line
        std::ostringstream where;
        where << path << ":" << lineNo << ": ";
        if (f.size() != columns) {
            where << "expected " << columns << " columns";
            return where.str();
        }
        std::vector<int32_t> v(columns);
        for (size_t c = 0; c < columns; ++c) {
            int64_t x = 0;
            if (!parseInt64(f[c], x) || x < INT32_MIN || x > INT32_MAX) {
                return where.str() + "'" + f[c] + "' is not a 32-bit integer";
            }
            v[c] = static_cast<int32_t>(x);
        }
        const std::string err = onRow(v);
        if (!err.empty()) return where.str() + err;
    }
    return "";
}

}  // namespace

bool makeDirectories(const std::string& path) {
    for (size_t i = 0; i < path.size(); ++i) {
        if (isSeparator(path[i]) && i > 0) makeOne(path.substr(0, i));
    }
    if (!path.empty()) makeOne(path);
    return directoryExists(path);
}

std::string baseName(const std::string& path) {
    size_t end = path.size();
    while (end > 0 && isSeparator(path[end - 1])) --end;
    size_t start = end;
    while (start > 0 && !isSeparator(path[start - 1])) --start;
    return path.substr(start, end - start);
}

std::string writeInstance(const std::string& dir, const Instance& inst,
                          const std::string& config_text) {
    const std::string invalid = validateInstance(inst);
    if (!invalid.empty()) return "refusing to write invalid instance: " + invalid;
    if (!makeDirectories(dir)) return "cannot create directory " + dir;

    {
        std::ofstream out(joinPath(dir, "agents.csv").c_str());
        if (!out) return "cannot write " + joinPath(dir, "agents.csv");
        out << "agent_id,group_id\n";
        for (int32_t a = 0; a < inst.num_agents; ++a) out << a << ',' << inst.group_id[a] << '\n';
    }
    {
        std::ofstream out(joinPath(dir, "resources.csv").c_str());
        if (!out) return "cannot write " + joinPath(dir, "resources.csv");
        out << "resource_id,capacity\n";
        for (int32_t r = 0; r < inst.num_resources; ++r) out << r << ',' << inst.capacity[r] << '\n';
    }
    {
        std::ofstream out(joinPath(dir, "edges.csv").c_str());
        if (!out) return "cannot write " + joinPath(dir, "edges.csv");
        out << "agent_id,resource_id,utility\n";
        for (size_t i = 0; i < inst.edges.size(); ++i) {
            out << inst.edges[i].agent_id << ',' << inst.edges[i].resource_id << ','
                << inst.edges[i].utility << '\n';
        }
    }
    if (!config_text.empty()) {
        std::ofstream out(joinPath(dir, "config.txt").c_str());
        if (!out) return "cannot write " + joinPath(dir, "config.txt");
        out << config_text;
    }
    return "";
}

std::string readInstance(const std::string& dir, Instance& out) {
    Instance inst;

    // Ids must be 0,1,2,... in order, so a row's id doubles as its position.
    std::string err = readCsv(joinPath(dir, "agents.csv"), "agent_id,group_id", 2,
                              [&inst](const std::vector<int32_t>& v) -> std::string {
                                  if (v[0] != static_cast<int32_t>(inst.group_id.size())) {
                                      return "agent_id must count up from 0 without gaps";
                                  }
                                  inst.group_id.push_back(v[1]);
                                  return "";
                              });
    if (!err.empty()) return err;

    err = readCsv(joinPath(dir, "resources.csv"), "resource_id,capacity", 2,
                  [&inst](const std::vector<int32_t>& v) -> std::string {
                      if (v[0] != static_cast<int32_t>(inst.capacity.size())) {
                          return "resource_id must count up from 0 without gaps";
                      }
                      inst.capacity.push_back(v[1]);
                      return "";
                  });
    if (!err.empty()) return err;

    err = readCsv(joinPath(dir, "edges.csv"), "agent_id,resource_id,utility", 3,
                  [&inst](const std::vector<int32_t>& v) -> std::string {
                      inst.edges.push_back({v[0], v[1], v[2]});
                      return "";
                  });
    if (!err.empty()) return err;

    inst.num_agents = static_cast<int32_t>(inst.group_id.size());
    inst.num_resources = static_cast<int32_t>(inst.capacity.size());
    inst.num_groups = 1;
    for (size_t a = 0; a < inst.group_id.size(); ++a) {
        inst.num_groups = std::max(inst.num_groups, inst.group_id[a] + 1);
    }

    err = validateInstance(inst);
    if (!err.empty()) return dir + ": " + err;
    out = std::move(inst);
    return "";
}

}  // namespace daa
