// The decor pictures the program comes with can be read (stb_image), without a window.

#include "readycad/core/Decor.hpp"
#include "readycad/render/Image.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace rc;

TEST_CASE("Every picture of the default decors is a readable square image", "[render][image][decor]")
{
    const std::filesystem::path folder = std::filesystem::path(READYCAD_ASSETS_DIR) / "textures";
    for (const core::Decor& decor : core::defaultDecorCatalog()) {
        if (decor.texture.empty()) {
            continue;
        }
        INFO(decor.texture);
        const auto image = render::loadImage(folder / decor.texture);
        REQUIRE(image.has_value());
        CHECK(image->width == image->height); // seamless tiles
        CHECK(image->width >= 256);
        CHECK(image->rgb.size() == static_cast<std::size_t>(image->width) * image->height * 3);
    }
}

TEST_CASE("A missing or broken picture gives nothing, not a crash", "[render][image]")
{
    CHECK_FALSE(render::loadImage("no/such/picture.jpg").has_value());
    const std::filesystem::path notAnImage = std::filesystem::path(READYCAD_ASSETS_DIR) / "textures" / "SOURCES.txt";
    CHECK_FALSE(render::loadImage(notAnImage).has_value());
}
