#pragma once

// A small G-code interpreter for the tests: reads what the postprocessor wrote and replays it
// as straight moves, independent of the code that wrote it. Knows what our programs use:
// G0, G1, G81 with G99, G80, X Y Z R F, T, M6, line numbers, ( ) and ; comments, %.

#include "readycad/core/Math.hpp"

#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc::test {

using core::Real;
using core::Vec3;

struct Move {
    bool rapid = true;
    Vec3 from{0.0};
    Vec3 to{0.0};
    int tool = 0;     ///< loaded tool, 0 = none yet
    bool drill = false; ///< part of a drill cycle
};

struct Replay {
    std::vector<std::string> lines; ///< without line numbers, comments and %; not empty
    std::vector<Move> moves;
    std::vector<int> tools;         ///< tool numbers in the order they were loaded
    bool percentFirst = false;
    bool percentLast = false;
    bool lineNumbers = false;

    [[nodiscard]] std::size_t indexOf(const std::string& line) const
    {
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (lines[i] == line) {
                return i;
            }
        }
        return lines.size();
    }
};

inline bool known(const Vec3& v)
{
    return !std::isnan(v.x) && !std::isnan(v.y) && !std::isnan(v.z);
}

inline Replay replay(const std::string& text)
{
    Replay r;
    constexpr Real nan = std::numeric_limits<Real>::quiet_NaN();
    Vec3 at(nan, nan, nan);
    int motion = 0;   // 0 rapid, 1 feed, 81 drill cycle
    int tool = 0;
    int pendingTool = 0;
    Real cycleZ = 0.0;
    Real cycleR = 0.0;
    std::vector<std::string> raw;
    std::string line;
    for (const char c : text + "\n") {
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            raw.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    while (!raw.empty() && raw.back().empty()) {
        raw.pop_back();
    }
    r.percentFirst = !raw.empty() && raw.front() == "%";
    r.percentLast = !raw.empty() && raw.back() == "%";

    for (std::string l : raw) {
        if (l == "%") {
            continue;
        }
        if (l.size() > 1 && l[0] == 'N' && std::isdigit(static_cast<unsigned char>(l[1])) != 0) {
            r.lineNumbers = true;
            l = l.substr(l.find(' ') + 1);
        }
        if (const auto semicolon = l.find(';'); semicolon != std::string::npos) {
            l = l.substr(0, semicolon);
        }
        if (const auto open = l.find('('); open != std::string::npos) {
            l = l.substr(0, open);
        }
        while (!l.empty() && l.back() == ' ') {
            l.pop_back();
        }
        if (l.empty()) {
            continue;
        }
        r.lines.push_back(l);

        // Words: letter + number.
        Vec3 target = at;
        bool moved = false;
        bool cycleLine = false;
        std::size_t i = 0;
        while (i < l.size()) {
            const char letter = l[i];
            if (letter == ' ') {
                ++i;
                continue;
            }
            std::size_t end = i + 1;
            while (end < l.size() && l[end] != ' ') {
                ++end;
            }
            const Real value = std::stod(l.substr(i + 1, end - i - 1));
            switch (letter) {
            case 'G': {
                const int g = static_cast<int>(value);
                if (g == 0 || g == 1) {
                    motion = g;
                } else if (g == 81) {
                    motion = 81;
                    cycleLine = true;
                } else if (g == 80) {
                    motion = 0;
                }
                break;
            }
            case 'X': target.x = value; moved = true; break;
            case 'Y': target.y = value; moved = true; break;
            case 'Z':
                if (motion == 81) {
                    cycleZ = value;
                } else {
                    target.z = value;
                    moved = true;
                }
                break;
            case 'R': cycleR = value; break;
            case 'T': pendingTool = static_cast<int>(value); break;
            case 'M':
                if (static_cast<int>(value) == 6) {
                    tool = pendingTool;
                    r.tools.push_back(tool);
                }
                break;
            default: break;
            }
            i = end;
        }
        (void)cycleLine;
        if (!moved) {
            continue;
        }
        if (motion == 81) {
            // Position at the current height, down to R, drill, back up to R (G99).
            const Vec3 above(target.x, target.y, at.z);
            r.moves.push_back({true, at, above, tool, true});
            const Vec3 atR(target.x, target.y, cycleR);
            r.moves.push_back({true, above, atR, tool, true});
            const Vec3 bottom(target.x, target.y, cycleZ);
            r.moves.push_back({false, atR, bottom, tool, true});
            r.moves.push_back({true, bottom, atR, tool, true});
            at = atR;
        } else {
            r.moves.push_back({motion == 0, at, target, tool, false});
            at = target;
        }
    }
    return r;
}

} // namespace rc::test
