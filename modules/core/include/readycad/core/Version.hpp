#pragma once

#include <string_view>

namespace rc::core {

/// Semantic version of ReadyCAD, taken from project(VERSION ...) in the root CMakeLists.txt.
struct Version {
    int major;
    int minor;
    int patch;
};

[[nodiscard]] Version version() noexcept;
[[nodiscard]] std::string_view versionString() noexcept;

} // namespace rc::core
