#include "readycad/export/SheetDrawing.hpp"

#include "Utf8.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/i18n/Labels.hpp"

#include <algorithm>
#include <cstdio>
#include <system_error>

namespace rc::exporting {

using i18n::TextId;

namespace {

std::string sizeText(Real a, Real b)
{
    return i18n::mm(a) + " × " + i18n::mm(b);
}

Layer sheetLayer()
{
    return {std::string(layers::kSheet), 8, {90, 90, 90}, LineType::Continuous, LineWeight::Normal};
}

Layer remnantLayer()
{
    return {std::string(layers::kRemnant), 3, {0, 140, 60}, LineType::Dashed, LineWeight::Normal};
}

Layer sawCutLayer()
{
    return {std::string(layers::kSawCut), 1, {210, 30, 30}, LineType::Continuous, LineWeight::Thin};
}

Layer fillLayer()
{
    return {std::string(layers::kPartFill), 9, {222, 231, 243}, LineType::Continuous,
            LineWeight::Thin};
}

Layer textLayer()
{
    return {std::string(layers::kText), 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thin};
}

/// The blank's outline when the part is not drawn in detail. On a map for people it is the
/// main line; in a DXF it is what the CAM cuts, styled like in the part drawings.
Layer outlineLayer(bool map)
{
    return map ? Layer{std::string(layers::kCutContour), 7, {0, 0, 0}, LineType::Continuous,
                       LineWeight::Normal}
               : Layer{std::string(layers::kCutContour), 8, {150, 150, 150},
                       LineType::Continuous, LineWeight::Thin};
}

/// Up to two lines of text centred in `box`, read along its longer side, shrunk to fit.
/// The first line is bold. Lines that would be smaller than `minimum` are left out.
void label(Drawing& d, std::size_t layer, const nesting::Rect& box, const std::string& first,
           const std::string& second, Real height, Real minimum)
{
    const ApproxTextMetrics metrics;
    const bool vertical = box.height() > box.width() * 1.15;
    const Real along = (vertical ? box.height() : box.width()) * 0.88;
    const Real across = (vertical ? box.width() : box.height()) * 0.8;
    const Real big = height * 1.25;
    const Real small = height * 0.8;
    const bool two = !second.empty();

    Real s = 1.0;
    const Real wide = std::max(metrics.width(first, big, true),
                               two ? metrics.width(second, small, false) : 0.0);
    if (wide > along) {
        s = along / wide;
    }
    const Real tall = (two ? big + small : big) * 1.45;
    if (tall * s > across) {
        s = across / tall;
    }
    const Real angle = vertical ? 90.0 : 0.0;
    const Vec2 centre = (box.min + box.max) * 0.5;
    const Vec2 up = vertical ? Vec2(-1.0, 0.0) : Vec2(0.0, 1.0);
    if (two && small * s >= minimum) {
        const Real gap = 0.25 * big * s;
        d.text(layer, {centre + up * gap, first, big * s, angle, HAlign::Center, VAlign::Bottom,
                       true});
        d.text(layer, {centre - up * gap, second, small * s, angle, HAlign::Center, VAlign::Top,
                       false});
        return;
    }
    // Only the label: let it use the room of both lines.
    Real only = big;
    if (metrics.width(first, only, true) > along) {
        only *= along / metrics.width(first, only, true);
    }
    only = std::min(only, across / 1.45);
    if (only >= minimum) {
        d.text(layer, {centre, first, only, angle, HAlign::Center, VAlign::Middle, true});
    }
}

std::string safeFileText(std::string_view text)
{
    std::string out;
    for (const char c : asciiFold(text)) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                       || c == '-';
        out += safe ? c : '_';
    }
    return out;
}

} // namespace

std::string placedPartName(const nesting::NestingResult& result, std::size_t part,
                           const PartSource* parts)
{
    const nesting::NestingPart& p = result.parts.at(part);
    if (parts != nullptr && p.cutListRow && *p.cutListRow < parts->cutList.size()) {
        return partName(parts->cutList[*p.cutListRow]);
    }
    return p.label;
}

Drawing drawSheetLayout(const nesting::NestingResult& result, std::size_t index,
                        const PartSource* parts, const SheetDrawingOptions& options)
{
    const nesting::SheetLayout& layout = result.layouts.at(index);
    const Real h = options.textHeight;
    const Real length = layout.sheet.length;
    const Real width = layout.sheet.width;
    Drawing d;
    const std::size_t sheet = d.addLayer(sheetLayer());
    const std::size_t fill = options.fills ? d.addLayer(fillLayer()) : 0;
    d.rectangle(sheet, Vec2(0.0), Vec2(length, width));

    // Parts: shading first, then lines, labels on top.
    std::vector<std::pair<nesting::Rect, std::size_t>> labels;
    for (const nesting::PlacedPart& placed : layout.parts) {
        const nesting::NestingPart& part = result.parts.at(placed.part);
        const nesting::Polygon outline = nesting::transformed(part.outline, placed.transform);
        if (options.fills) {
            d.add(fill, Polyline{outline, true, true});
        }
        if (options.partDetails && parts != nullptr && part.cutListRow) {
            PartDrawingOptions po;
            po.textHeight = h * 0.6;
            po.cutContour = true;
            po.dimensions = false;
            const detailing::FlatPart flat =
                detailing::flatPartOf(parts->product, parts->cutList.at(*part.cutListRow));
            d.append(drawPart(flat, po),
                     Transform2D{1.0, placed.transform.degrees(), placed.transform.offset});
        } else {
            d.add(d.addLayer(outlineLayer(!options.partDetails)), Polyline{outline, true, false});
        }
        labels.emplace_back(nesting::labelArea(outline), placed.part);
    }

    for (const nesting::Rect& r : layout.remnants) {
        const std::size_t layer = d.addLayer(remnantLayer());
        d.rectangle(layer, r.min, r.max);
        label(d, layer, r, std::string(i18n::text(TextId::Remnant)), sizeText(r.width(), r.height()),
              h * 0.9, h * 0.4);
    }
    if (options.sawCuts && !layout.cuts.empty()) {
        const std::size_t layer = d.addLayer(sawCutLayer());
        for (const nesting::SawCut& cut : layout.cuts) {
            d.line(layer, cut.from, cut.to);
        }
    }

    const std::size_t text = d.addLayer(textLayer());
    for (const auto& [box, part] : labels) {
        const nesting::Rect blank = nesting::bounds(result.parts[part].outline);
        label(d, text, box, result.parts[part].label,
              placedPartName(result, part, parts) + "  " + sizeText(blank.width(), blank.height()),
              h, h * 0.45);
    }
    if (!options.title.empty()) {
        d.text(text, {Vec2(0.0, width + 0.8 * h), options.title, 1.4 * h, 0.0, HAlign::Left,
                      VAlign::Bottom, true});
    }
    return d;
}

std::string sheetTitle(const nesting::NestingResult& result, std::size_t index)
{
    const nesting::SheetLayout& layout = result.layouts.at(index);
    std::string title = std::string(i18n::text(TextId::SheetLabel)) + " "
                      + std::to_string(index + 1) + " / " + std::to_string(result.layouts.size())
                      + " · " + layout.sheet.material.name + " · "
                      + sizeText(layout.sheet.length, layout.sheet.width);
    if (layout.count > 1) {
        title += " · ×" + std::to_string(layout.count);
    }
    return title;
}

std::string sheetFileName(const nesting::NestingResult& result, std::size_t index)
{
    const nesting::SheetLayout& layout = result.layouts.at(index);
    char prefix[16];
    std::snprintf(prefix, sizeof prefix, "%02zu_", index + 1);
    std::string name = prefix + safeFileText(layout.sheet.material.name) + "_"
                     + i18n::mm(layout.sheet.length) + "x" + i18n::mm(layout.sheet.width);
    if (layout.count > 1) {
        name += "_x" + std::to_string(layout.count);
    }
    return name + ".dxf";
}

std::vector<std::filesystem::path> saveSheetDxfs(const nesting::NestingResult& result,
                                                 const PartSource* parts,
                                                 const std::filesystem::path& folder,
                                                 const SheetDrawingOptions& options)
{
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        throw ExportError("cannot create folder " + pathToUtf8(folder) + ": " + ec.message());
    }
    std::vector<std::filesystem::path> files;
    for (std::size_t i = 0; i < result.layouts.size(); ++i) {
        SheetDrawingOptions o = options;
        if (o.title.empty()) {
            o.title = sheetTitle(result, i);
        }
        const std::filesystem::path file = folder / sheetFileName(result, i);
        saveDxf(drawSheetLayout(result, i, parts, o), file);
        files.push_back(file);
    }
    return files;
}

} // namespace rc::exporting
