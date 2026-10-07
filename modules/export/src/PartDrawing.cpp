#include "readycad/export/PartDrawing.hpp"

#include "Utf8.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/i18n/Labels.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <system_error>

namespace rc::exporting {

using detailing::FlatPart;
using detailing::FlatSide;

namespace {

/// 5 -> "5", 12.5 -> "12p5", 0.4 -> "0p4"
std::string layerNumber(Real value)
{
    std::string text = i18n::mm(value);
    std::replace(text.begin(), text.end(), '.', 'p');
    return text;
}

Layer drillLayer(FlatSide side, Real diameter, Real depth)
{
    Layer layer;
    layer.name = drillLayerName(side, diameter, depth);
    switch (side) {
    case FlatSide::Near:
        layer.aci = 3;
        layer.print = {0, 0, 0};
        break;
    case FlatSide::Far:
        layer.aci = 5;
        layer.print = {110, 110, 110};
        layer.lineType = LineType::Dashed;
        layer.weight = LineWeight::Thin;
        break;
    case FlatSide::Edge:
        layer.aci = 6;
        layer.print = {40, 70, 150};
        layer.lineType = LineType::Dashed;
        layer.weight = LineWeight::Thin;
        break;
    }
    return layer;
}

Layer grooveLayer(FlatSide side, Real width, Real depth)
{
    Layer layer;
    layer.name = grooveLayerName(side, width, depth);
    layer.aci = 4;
    layer.print = {0, 90, 160};
    if (side != FlatSide::Near) {
        layer.lineType = LineType::Dashed;
        layer.weight = LineWeight::Thin;
    }
    return layer;
}

/// The rectangle around a centre line from `a` to `b`, `width` wide.
std::vector<Vec2> band(Vec2 a, Vec2 b, Real width)
{
    const Vec2 d = b - a;
    const Real length = glm::length(d);
    const Vec2 dir = length > core::kEpsilon ? d / length : Vec2(1.0, 0.0);
    const Vec2 n = Vec2(-dir.y, dir.x) * (width / 2.0);
    return {a + n, a - n, b - n, b + n};
}

std::string sizeText(Real a, Real b)
{
    return i18n::mm(a) + " × " + i18n::mm(b);
}

std::string partTitle(std::size_t number, const detailing::CutListRow& row)
{
    return std::to_string(number) + ". " + partName(row) + "  ×" + std::to_string(row.quantity)
         + "  " + sizeText(row.finishLength, row.finishWidth) + "  " + row.material.name;
}

} // namespace

std::string drillLayerName(FlatSide side, Real diameter, Real depth)
{
    const char* prefix = side == FlatSide::Near ? "DRILL_D"
                       : side == FlatSide::Far  ? "DRILL_BACK_D"
                                                : "DRILL_EDGE_D";
    return prefix + layerNumber(diameter) + "_Z" + layerNumber(depth);
}

std::string grooveLayerName(FlatSide side, Real width, Real depth)
{
    return (side == FlatSide::Near ? "GROOVE_W" : "GROOVE_BACK_W") + layerNumber(width) + "_Z"
         + layerNumber(depth);
}

Drawing drawPart(const FlatPart& part, const PartDrawingOptions& options)
{
    Drawing drawing;
    const Real h = options.textHeight;
    const Real l = part.length;
    const Real w = part.width;

    const std::size_t contour = drawing.addLayer(
        {std::string(layers::kContour), 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thick});
    drawing.rectangle(contour, Vec2(0.0), Vec2(l, w));

    if (options.cutContour) {
        const std::size_t cut = drawing.addLayer({std::string(layers::kCutContour), 8,
                                                  {150, 150, 150}, LineType::Continuous,
                                                  LineWeight::Thin});
        drawing.rectangle(cut, part.cut.min, part.cut.max);
    }

    // Holes and grooves, one layer per tool and depth.
    for (const detailing::FlatHole& hole : part.holes) {
        const std::size_t layer = drawing.addLayer(drillLayer(hole.side, hole.diameter, hole.depth));
        if (hole.side == FlatSide::Edge) {
            drawing.add(layer, Polyline{band(hole.center, hole.center + hole.direction * hole.depth,
                                             hole.diameter),
                                        true, false});
        } else {
            drawing.circle(layer, hole.center, hole.diameter / 2.0);
        }
    }
    for (const detailing::FlatGroove& groove : part.grooves) {
        const std::size_t layer = drawing.addLayer(grooveLayer(groove.side, groove.width, groove.depth));
        drawing.add(layer, Polyline{band(groove.start, groove.end, groove.width), true, false});
    }

    const std::size_t text = drawing.addLayer(
        {std::string(layers::kText), 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thin});

    // Edge names outside, band marks inside: the bander operator sees which edge gets what.
    Real labelSpace = 0.0;
    if (options.edgeBands) {
        const std::size_t marks = drawing.addLayer({std::string(layers::kEdgeBand), 1,
                                                    {200, 30, 30}, LineType::Continuous,
                                                    LineWeight::Thick});
        const Real inset = 0.35 * h;
        const Real gap = 0.5 * h;
        for (std::size_t i = 0; i < part.outline.size(); ++i) {
            const detailing::FlatEdge& edge = part.outline[i];
            const Vec2 along = glm::normalize(edge.end - edge.start);
            const Vec2 inward(-along.y, along.x); // the outline runs counter-clockwise
            if (edge.band.present()) {
                drawing.line(marks, edge.start + inward * inset + along * inset,
                             edge.end + inward * inset - along * inset);
            }
            std::string label(i18n::text(i18n::edgeText(edge.side)));
            if (edge.band.present()) {
                label += " · " + edge.band.name;
            }
            Text t;
            t.text = label;
            t.height = h * 0.8;
            t.position = (edge.start + edge.end) * 0.5 - inward * gap;
            t.hAlign = HAlign::Center;
            // Texts read left to right or bottom to top; their "up" points away from the part
            // on the bottom and the left side.
            if (i == 0) { // bottom
                t.vAlign = VAlign::Top;
            } else if (i == 1) { // right
                t.angle = 90.0;
                t.vAlign = VAlign::Top;
            } else if (i == 2) { // top
                t.vAlign = VAlign::Bottom;
            } else { // left
                t.angle = 90.0;
                t.vAlign = VAlign::Bottom;
            }
            drawing.text(text, std::move(t));
        }
        labelSpace = gap + 1.25 * h;
    }

    if (options.dimensions) {
        const std::size_t dims = drawing.addLayer({std::string(layers::kDimensions), 2,
                                                   {40, 40, 40}, LineType::Continuous,
                                                   LineWeight::Thin});
        const Real offset = labelSpace + 2.0 * h;
        drawing.dimension(dims, {Vec2(0.0, 0.0), Vec2(l, 0.0), -offset, "", h, h});
        drawing.dimension(dims, {Vec2(0.0, 0.0), Vec2(0.0, w), offset, "", h, h});
    }

    // Origin of the hole coordinates.
    drawing.circle(text, Vec2(0.0), 0.4 * h);
    drawing.text(text, {Vec2(0.6 * h, 0.6 * h), "0", 0.7 * h, 0.0, HAlign::Left,
                        VAlign::Bottom, false});

    if (!options.title.empty()) {
        drawing.text(text, {Vec2(0.0, w + labelSpace + 1.0 * h), options.title, 1.2 * h, 0.0,
                            HAlign::Left, VAlign::Bottom, true});
    }
    return drawing;
}

// ---------------------------------------------------------------------------------------------

std::string partName(const detailing::CutListRow& row)
{
    return row.name.empty() ? std::string(i18n::text(i18n::kindText(row.kind))) : row.name;
}

FlatPart flatPartOf(const core::Product& product, const detailing::CutListRow& row)
{
    if (row.components.empty()) {
        throw std::invalid_argument("cut list row without components");
    }
    const core::FurnitureComponent* component = product.find(row.components.front());
    if (component == nullptr || !component->hasPanel()) {
        throw std::invalid_argument("cut list row does not belong to this product");
    }
    return detailing::flatten(*component->panel(), product.rules());
}

Drawing drawAllParts(const core::Product& product,
                     const std::vector<detailing::CutListRow>& cutList,
                     const PartsSheetOptions& options)
{
    Drawing sheet;
    Real x = 0.0;
    Real y = 0.0;
    Real rowHeight = 0.0;
    const ApproxTextMetrics metrics;
    for (std::size_t i = 0; i < cutList.size(); ++i) {
        const detailing::CutListRow& row = cutList[i];
        PartDrawingOptions partOptions;
        partOptions.textHeight = options.textHeight;
        partOptions.cutContour = options.cutContour;
        partOptions.title = partTitle(i + 1, row);
        const Drawing part = drawPart(flatPartOf(product, row), partOptions);
        const Bounds b = part.bounds(metrics);
        if (x > 0.0 && x + b.size().x > options.rowWidth) {
            x = 0.0;
            y -= rowHeight + options.gap;
            rowHeight = 0.0;
        }
        // Rows grow downwards; every part keeps its own origin in the lower-left corner.
        sheet.append(part, 1.0, Vec2(x - b.min.x, y - b.max.y));
        x += b.size().x + options.gap;
        rowHeight = std::max(rowHeight, b.size().y);
    }
    return sheet;
}

std::string partFileName(std::size_t number, const detailing::CutListRow& row)
{
    char prefix[16];
    std::snprintf(prefix, sizeof prefix, "%02zu_", number);
    std::string name;
    for (const char c : asciiFold(partName(row))) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                       || c == '-';
        name += safe ? c : '_';
    }
    return prefix + name + "_" + i18n::mm(row.cut.length) + "x" + i18n::mm(row.cut.width) + ".dxf";
}

std::vector<std::filesystem::path> savePartDxfs(const core::Product& product,
                                                const std::vector<detailing::CutListRow>& cutList,
                                                const std::filesystem::path& folder,
                                                const PartsSheetOptions& options)
{
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        throw ExportError("cannot create folder " + pathToUtf8(folder) + ": " + ec.message());
    }
    std::vector<std::filesystem::path> files;
    for (std::size_t i = 0; i < cutList.size(); ++i) {
        PartDrawingOptions partOptions;
        partOptions.textHeight = options.textHeight;
        partOptions.cutContour = options.cutContour;
        partOptions.title = partTitle(i + 1, cutList[i]);
        const std::filesystem::path file = folder / partFileName(i + 1, cutList[i]);
        saveDxf(drawPart(flatPartOf(product, cutList[i]), partOptions), file);
        files.push_back(file);
    }
    return files;
}

} // namespace rc::exporting
