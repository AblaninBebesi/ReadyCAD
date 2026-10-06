#include "Fonts.hpp"

#include <imgui.h>

#include <array>
#include <filesystem>

namespace rc::ui {

bool loadUiFonts(float sizePixels)
{
    // TODO(stage 10): ship an OFL font (e.g. Noto Sans) in assets/fonts to look identical everywhere.
    constexpr std::array kCandidates = {
        "assets/fonts/NotoSans-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };

    ImGuiIO& io = ImGui::GetIO();
    for (const char* path : kCandidates) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            continue;
        }
        // ImGui >= 1.92 rasterises glyphs on demand, so no glyph ranges are needed for Turkish.
        if (io.Fonts->AddFontFromFileTTF(path, sizePixels) != nullptr) {
            return true;
        }
    }
    io.Fonts->AddFontDefault();
    return false;
}

} // namespace rc::ui
