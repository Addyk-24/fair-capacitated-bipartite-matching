#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>

namespace daa {

// Minimal "--key value" / "--flag" parser shared by the command-line tools.
class Args {
public:
    // Returns "" on success, otherwise a message about the malformed argument.
    std::string parse(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            const std::string token = argv[i];
            if (token.size() < 3 || token.compare(0, 2, "--") != 0) {
                return "unexpected argument '" + token + "'";
            }
            const std::string key = token.substr(2);
            if (i + 1 < argc && std::string(argv[i + 1]).compare(0, 2, "--") != 0) {
                values_[key] = argv[++i];
            } else {
                values_[key] = "";  // bare flag
            }
        }
        return "";
    }

    bool has(const std::string& key) const { return values_.count(key) > 0; }

    std::string get(const std::string& key, const std::string& fallback = "") const {
        std::map<std::string, std::string>::const_iterator it = values_.find(key);
        return it == values_.end() ? fallback : it->second;
    }

    const std::map<std::string, std::string>& all() const { return values_; }

private:
    std::map<std::string, std::string> values_;
};

inline bool parseInt64(const std::string& s, int64_t& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

inline bool parseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(s.c_str(), &end);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

}  // namespace daa
