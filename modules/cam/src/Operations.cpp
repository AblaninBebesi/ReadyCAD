#include "readycad/cam/Operations.hpp"

#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Utf8.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace rc::cam {

Vec2 planNormal(Face face) noexcept
{
    switch (face) {
    case Face::Left:  return {-1.0, 0.0};
    case Face::Right: return {1.0, 0.0};
    case Face::Front: return {0.0, -1.0};
    case Face::Back:  return {0.0, 1.0};
    case Face::Top:
    case Face::Bottom: break;
    }
    return Vec2(0.0);
}

Face edgeFace(Vec2 normal) noexcept
{
    if (std::abs(normal.x) >= std::abs(normal.y)) {
        return normal.x < 0.0 ? Face::Left : Face::Right;
    }
    return normal.y < 0.0 ? Face::Front : Face::Back;
}

Vec3 inward(Face face) noexcept
{
    switch (face) {
    case Face::Top:    return {0.0, 0.0, -1.0};
    case Face::Bottom: return {0.0, 0.0, 1.0};
    default: break;
    }
    const Vec2 n = planNormal(face);
    return {-n.x, -n.y, 0.0};
}

PartMachining partMachining(const detailing::FlatPart& part, const nesting::Polygon& blank,
                            std::string label, std::string name)
{
    using detailing::FlatSide;
    PartMachining out;
    out.label = std::move(label);
    out.name = std::move(name);
    out.thickness = part.thickness;
    const Real t = part.thickness;
    for (const detailing::FlatHole& h : part.holes) {
        Drilling d;
        d.diameter = h.diameter;
        d.depth = h.depth;
        d.fastener = h.fastener;
        switch (h.side) {
        case FlatSide::Near:
            d.face = Face::Top; // the view face lies up on the machine
            d.entry = {h.center.x, h.center.y, t};
            break;
        case FlatSide::Far:
            d.face = Face::Bottom;
            d.entry = {h.center.x, h.center.y, 0.0};
            break;
        case FlatSide::Edge:
            d.face = edgeFace(-h.direction); // the hole goes in, the face looks out
            d.entry = {h.center.x, h.center.y, t - h.height};
            break;
        }
        out.operations.emplace_back(d);
    }
    for (const detailing::FlatGroove& g : part.grooves) {
        out.operations.emplace_back(Groove{g.side == FlatSide::Near ? Face::Top : Face::Bottom,
                                           g.start, g.end, g.width, g.depth});
    }
    out.operations.emplace_back(Contour{nesting::normalized(blank)});
    return out;
}

Operation transformed(const Operation& operation, const nesting::Transform& t)
{
    return std::visit(
        [&](const auto& op) -> Operation {
            using T = std::decay_t<decltype(op)>;
            T copy = op;
            if constexpr (std::is_same_v<T, Drilling>) {
                const Vec2 at = t.apply(Vec2(op.entry.x, op.entry.y));
                copy.entry = {at.x, at.y, op.entry.z};
                if (op.face != Face::Top && op.face != Face::Bottom) {
                    copy.face = edgeFace(nesting::rotateQuarter(planNormal(op.face), t.quarterTurns));
                }
            } else if constexpr (std::is_same_v<T, Groove>) {
                copy.start = t.apply(op.start);
                copy.end = t.apply(op.end);
            } else {
                copy.outline = nesting::transformed(op.outline, t);
            }
            return copy;
        },
        operation);
}

namespace {

std::string safeName(std::string_view text)
{
    std::string out;
    for (const char c : i18n::asciiFold(text)) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                       || c == '-';
        out += safe ? c : '_';
    }
    return out;
}

std::string rowName(const detailing::CutListRow& row)
{
    return row.name.empty() ? std::string(i18n::text(i18n::kindText(row.kind))) : row.name;
}

} // namespace

SheetJob sheetJob(const nesting::NestingResult& result, std::size_t index,
                  const detailing::PartSource* parts)
{
    const nesting::SheetLayout& layout = result.layouts.at(index);
    SheetJob job;
    char prefix[16];
    std::snprintf(prefix, sizeof prefix, "%02zu_", index + 1);
    job.name = prefix + safeName(layout.sheet.material.name) + "_" + i18n::mm(layout.sheet.length)
             + "x" + i18n::mm(layout.sheet.width);
    if (layout.count > 1) {
        job.name += "_x" + std::to_string(layout.count);
    }
    job.material = layout.sheet.material.name;
    job.length = layout.sheet.length;
    job.width = layout.sheet.width;
    job.thickness = layout.sheet.material.thickness;
    job.count = layout.count;
    job.partSpacing = result.settings.router.spacing();

    for (const nesting::PlacedPart& placed : layout.parts) {
        const nesting::NestingPart& part = result.parts.at(placed.part);
        PartMachining machining;
        if (parts != nullptr && part.cutListRow && *part.cutListRow < parts->cutList.size()) {
            const detailing::CutListRow& row = parts->cutList[*part.cutListRow];
            machining = partMachining(detailing::flatPartOf(parts->product, row), part.outline,
                                      part.label, rowName(row));
        } else {
            machining.label = part.label;
            machining.name = part.label;
            machining.thickness = job.thickness;
            machining.operations.emplace_back(Contour{nesting::normalized(part.outline)});
        }
        for (Operation& op : machining.operations) {
            op = transformed(op, placed.transform);
        }
        job.parts.push_back(std::move(machining));
    }
    return job;
}

} // namespace rc::cam
