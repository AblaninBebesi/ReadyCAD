#include "readycad/render/Image.hpp"

#include <stb_image.h>

#include <fstream>
#include <iterator>
#include <memory>

namespace rc::render {

std::optional<Image> loadImage(const std::filesystem::path& path)
{
    // The file is read here, not by stb: std::filesystem handles Turkish folder names on Windows.
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                           std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        return std::nullopt;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    const std::unique_ptr<stbi_uc, void (*)(void*)> pixels(
        stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height,
                              &channels, 3),
        stbi_image_free);
    if (!pixels || width <= 0 || height <= 0) {
        return std::nullopt;
    }
    Image image;
    image.width = width;
    image.height = height;
    image.rgb.assign(pixels.get(), pixels.get() + static_cast<std::size_t>(width) * height * 3);
    return image;
}

} // namespace rc::render
