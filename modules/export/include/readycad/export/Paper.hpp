#pragma once

#include "readycad/export/Drawing.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rc::exporting {

/// ISO 216 sizes used for workshop documents.
enum class PaperSize { A4, A3, A2, A1 };
enum class Orientation { Portrait, Landscape };

struct PageFormat {
    PaperSize size = PaperSize::A4;
    Orientation orientation = Orientation::Landscape;
};

/// Width x height in mm, orientation applied (A4 landscape -> 297 x 210).
[[nodiscard]] Vec2 pageSize(const PageFormat& format) noexcept;

[[nodiscard]] std::string_view toString(PaperSize size) noexcept; // "A4"

/// Drawing frame per ISO 5457: 20 mm on the left for filing, 10 mm elsewhere.
struct Margins {
    Real left = 20.0;
    Real right = 10.0;
    Real top = 10.0;
    Real bottom = 10.0;
};

/// One printed page: its size and everything on it, in mm from the lower-left corner.
struct Page {
    Vec2 size{0.0};
    Drawing drawing;
    std::string bookmark; ///< entry in the PDF outline; empty = none
};

/// What the PDF writer prints: pages plus document information.
struct PageSet {
    std::string title;
    std::string author;
    std::string subject;
    std::vector<Page> pages;
};

} // namespace rc::exporting
