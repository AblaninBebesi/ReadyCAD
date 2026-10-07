#pragma once

// A tiny DXF reader for tests: enough to check what the writer produced, independent of it.

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc::test {

struct DxfPair {
    int code = 0;
    std::string value;
};

/// Splits a DXF file into (group code, value) pairs. Throws if a code is not an integer or the
/// number of lines is odd — both would make the file unreadable for CAD programs.
inline std::vector<DxfPair> parseDxf(const std::string& text)
{
    std::vector<std::string> lines;
    std::string line;
    for (const char c : text) {
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    if (lines.size() % 2 != 0) {
        throw std::runtime_error("odd number of lines");
    }
    std::vector<DxfPair> pairs;
    for (std::size_t i = 0; i < lines.size(); i += 2) {
        pairs.push_back({std::stoi(lines[i]), lines[i + 1]}); // throws if not a number
    }
    return pairs;
}

/// One object of a section: its type ("LINE", "LAYER", ...) and its groups.
struct DxfObject {
    std::string type;
    std::multimap<int, std::string> groups;

    [[nodiscard]] std::string get(int code) const
    {
        const auto it = groups.find(code);
        return it == groups.end() ? std::string() : it->second;
    }
    [[nodiscard]] double number(int code) const { return std::stod(get(code)); }
    [[nodiscard]] std::vector<std::string> all(int code) const
    {
        std::vector<std::string> values;
        const auto [first, last] = groups.equal_range(code);
        for (auto it = first; it != last; ++it) {
            values.push_back(it->second);
        }
        return values;
    }
};

/// Names of the sections in file order.
inline std::vector<std::string> dxfSections(const std::vector<DxfPair>& pairs)
{
    std::vector<std::string> names;
    for (std::size_t i = 0; i + 1 < pairs.size(); ++i) {
        if (pairs[i].code == 0 && pairs[i].value == "SECTION" && pairs[i + 1].code == 2) {
            names.push_back(pairs[i + 1].value);
        }
    }
    return names;
}

/// Objects (code 0 starts one) inside the named section; for TABLES all table entries.
inline std::vector<DxfObject> dxfObjects(const std::vector<DxfPair>& pairs,
                                         const std::string& section)
{
    std::vector<DxfObject> objects;
    bool inside = false;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const DxfPair& p = pairs[i];
        if (!inside) {
            inside = p.code == 2 && p.value == section && i > 0 && pairs[i - 1].value == "SECTION";
            continue;
        }
        if (p.code == 0) {
            if (p.value == "ENDSEC") {
                break;
            }
            objects.push_back({p.value, {}});
        } else if (!objects.empty()) {
            objects.back().groups.emplace(p.code, p.value);
        }
    }
    return objects;
}

/// Entries of one table of the TABLES section ("LAYER", "LTYPE", "STYLE").
inline std::vector<DxfObject> dxfTable(const std::vector<DxfPair>& pairs, const std::string& table)
{
    std::vector<DxfObject> entries;
    for (const DxfObject& object : dxfObjects(pairs, "TABLES")) {
        if (object.type == table) {
            entries.push_back(object);
        }
    }
    return entries;
}

} // namespace rc::test
