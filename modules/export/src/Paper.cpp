#include "readycad/export/Paper.hpp"

namespace rc::exporting {

Vec2 pageSize(const PageFormat& format) noexcept
{
    Vec2 portrait(210.0, 297.0);
    switch (format.size) {
    case PaperSize::A4: portrait = {210.0, 297.0}; break;
    case PaperSize::A3: portrait = {297.0, 420.0}; break;
    case PaperSize::A2: portrait = {420.0, 594.0}; break;
    case PaperSize::A1: portrait = {594.0, 841.0}; break;
    }
    return format.orientation == Orientation::Portrait ? portrait : Vec2(portrait.y, portrait.x);
}

std::string_view toString(PaperSize size) noexcept
{
    switch (size) {
    case PaperSize::A4: return "A4";
    case PaperSize::A3: return "A3";
    case PaperSize::A2: return "A2";
    case PaperSize::A1: return "A1";
    }
    return "A4";
}

} // namespace rc::exporting
