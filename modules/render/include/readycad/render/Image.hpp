#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace rc::render {

/// A picture in memory: rows from the top, 3 bytes (R, G, B) per pixel.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

/// Reads a JPEG or PNG file (stb_image); nullopt if it is missing or not such a picture.
/// GL-free, so the decor pictures can be checked without a window.
[[nodiscard]] std::optional<Image> loadImage(const std::filesystem::path& path);

} // namespace rc::render
