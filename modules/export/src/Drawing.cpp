#include "readycad/export/Drawing.hpp"

#include "readycad/i18n/Labels.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rc::exporting {

namespace {

constexpr Real kDescender = 0.25; // of the cap height, below the baseline

Vec2 rotate(Vec2 v, Real degrees) noexcept
{
    const Real r = degrees * std::numbers::pi / 180.0;
    const Real c = std::cos(r);
    const Real s = std::sin(r);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

/// Text box in the text's own axes: x along the baseline from its start, y up from it.
struct LocalBox {
    Real x0, x1, y0, y1;
};

LocalBox localBox(const Text& text, Real width) noexcept
{
    Real x0 = 0.0;
    switch (text.hAlign) {
    case HAlign::Left:   x0 = 0.0; break;
    case HAlign::Center: x0 = -width / 2.0; break;
    case HAlign::Right:  x0 = -width; break;
    }
    const Real h = text.height;
    Real baseline = 0.0; // relative to the insertion point
    switch (text.vAlign) {
    case VAlign::Baseline: baseline = 0.0; break;
    case VAlign::Bottom:   baseline = kDescender * h; break;
    case VAlign::Middle:   baseline = -h / 2.0; break;
    case VAlign::Top:      baseline = -h; break;
    }
    return {x0, x0 + width, baseline - kDescender * h, baseline + h};
}

Real baselineOffset(const Text& text) noexcept
{
    return localBox(text, 0.0).y0 + kDescender * text.height;
}

/// A filled arrow head with its tip at `tip`, pointing along `direction` (unit).
Polyline arrowHead(Vec2 tip, Vec2 direction, Real size)
{
    const Vec2 back = tip - direction * size;
    const Vec2 side = Vec2(-direction.y, direction.x) * (size / 6.0);
    return Polyline{{tip, back + side, back - side}, true, true};
}

Shape transformed(const Shape& shape, Real s, Vec2 offset)
{
    const auto p = [&](Vec2 v) { return v * s + offset; };
    return std::visit(
        [&](const auto& e) -> Shape {
            using T = std::decay_t<decltype(e)>;
            T copy = e;
            if constexpr (std::is_same_v<T, Line>) {
                copy.a = p(e.a);
                copy.b = p(e.b);
            } else if constexpr (std::is_same_v<T, Polyline>) {
                for (Vec2& point : copy.points) {
                    point = p(point);
                }
            } else if constexpr (std::is_same_v<T, Circle>) {
                copy.center = p(e.center);
                copy.radius = e.radius * s;
            } else if constexpr (std::is_same_v<T, Text>) {
                copy.position = p(e.position);
                copy.height = e.height * s;
            } else {
                if (copy.text.empty()) {
                    copy.text = dimensionText(e); // the real length, not the length on paper
                }
                copy.a = p(e.a);
                copy.b = p(e.b);
                copy.offset = e.offset * s;
                copy.textHeight = e.textHeight * s;
                copy.arrowSize = e.arrowSize * s;
            }
            return copy;
        },
        shape);
}

void addShape(Bounds& bounds, const Shape& shape, const TextMetrics& metrics)
{
    std::visit(
        [&](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Line>) {
                bounds.add(e.a);
                bounds.add(e.b);
            } else if constexpr (std::is_same_v<T, Polyline>) {
                for (const Vec2& point : e.points) {
                    bounds.add(point);
                }
            } else if constexpr (std::is_same_v<T, Circle>) {
                bounds.add(e.center - Vec2(e.radius));
                bounds.add(e.center + Vec2(e.radius));
            } else if constexpr (std::is_same_v<T, Text>) {
                for (const Vec2& corner : textCorners(e, metrics)) {
                    bounds.add(corner);
                }
            } else {
                for (const Shape& part : explode(e, metrics)) {
                    addShape(bounds, part, metrics);
                }
            }
        },
        shape);
}

} // namespace

// ---------------------------------------------------------------------------------------------

std::size_t utf8Length(std::string_view text) noexcept
{
    std::size_t n = 0;
    for (const char c : text) {
        // Count every byte except UTF-8 continuation bytes (10xxxxxx).
        n += (static_cast<unsigned char>(c) & 0xC0U) != 0x80U ? 1 : 0;
    }
    return n;
}

Real ApproxTextMetrics::width(std::string_view text, Real height, bool bold) const
{
    return static_cast<Real>(utf8Length(text)) * height * (bold ? 0.86 : 0.8);
}

void Bounds::add(Vec2 p) noexcept
{
    if (empty) {
        min = max = p;
        empty = false;
        return;
    }
    min = glm::min(min, p);
    max = glm::max(max, p);
}

void Bounds::add(const Bounds& other) noexcept
{
    if (!other.empty) {
        add(other.min);
        add(other.max);
    }
}

// ---------------------------------------------------------------------------------------------

std::size_t Drawing::addLayer(const Layer& layer)
{
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].name == layer.name) {
            return i;
        }
    }
    layers_.push_back(layer);
    return layers_.size() - 1;
}

std::size_t Drawing::layerIndex(std::string_view name) const
{
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].name == name) {
            return i;
        }
    }
    throw std::out_of_range("Drawing: no layer " + std::string(name));
}

bool Drawing::hasLayer(std::string_view name) const noexcept
{
    for (const Layer& layer : layers_) {
        if (layer.name == name) {
            return true;
        }
    }
    return false;
}

void Drawing::add(std::size_t layer, Shape shape)
{
    if (layer >= layers_.size()) {
        throw std::out_of_range("Drawing: layer index out of range");
    }
    entities_.push_back({layer, std::move(shape)});
}

void Drawing::rectangle(std::size_t layer, Vec2 min, Vec2 max, bool filled)
{
    add(layer, Polyline{{min, {max.x, min.y}, max, {min.x, max.y}}, true, filled});
}

void Drawing::append(const Drawing& other, Real scale, Vec2 offset)
{
    std::vector<std::size_t> remap;
    remap.reserve(other.layers_.size());
    for (const Layer& layer : other.layers_) {
        remap.push_back(addLayer(layer));
    }
    entities_.reserve(entities_.size() + other.entities_.size());
    for (const Entity& e : other.entities_) {
        entities_.push_back({remap[e.layer], transformed(e.shape, scale, offset)});
    }
}

Bounds Drawing::bounds(const TextMetrics& metrics) const
{
    Bounds result;
    for (const Entity& e : entities_) {
        addShape(result, e.shape, metrics);
    }
    return result;
}

// ---------------------------------------------------------------------------------------------

std::vector<Vec2> textCorners(const Text& text, const TextMetrics& metrics)
{
    const LocalBox box = localBox(text, metrics.width(text.text, text.height, text.bold));
    std::vector<Vec2> corners;
    for (const Vec2 local : {Vec2(box.x0, box.y0), Vec2(box.x1, box.y0), Vec2(box.x1, box.y1),
                             Vec2(box.x0, box.y1)}) {
        corners.push_back(text.position + rotate(local, text.angle));
    }
    return corners;
}

Vec2 baselineStart(const Text& text, const TextMetrics& metrics)
{
    const LocalBox box = localBox(text, metrics.width(text.text, text.height, text.bold));
    return text.position + rotate(Vec2(box.x0, baselineOffset(text)), text.angle);
}

std::string dimensionText(const Dimension& dimension)
{
    return i18n::mm(glm::length(dimension.b - dimension.a));
}

std::vector<Shape> explode(const Dimension& d, const TextMetrics& metrics)
{
    std::vector<Shape> out;
    const Vec2 ab = d.b - d.a;
    const Real length = glm::length(ab);
    if (length < core::kEpsilon) {
        return out;
    }
    const Vec2 dir = ab / length;
    const Vec2 normal(-dir.y, dir.x); // left of a -> b
    const Vec2 p1 = d.a + normal * d.offset;
    const Vec2 p2 = d.b + normal * d.offset;
    const Real h = d.textHeight;

    // Extension lines: a small gap at the object, a little past the dimension line (ISO 129).
    const Real side = d.offset >= 0.0 ? 1.0 : -1.0;
    const Real gap = 0.4 * h;
    const Real overshoot = 0.6 * h;
    if (std::abs(d.offset) > gap) {
        out.emplace_back(Line{d.a + normal * (side * gap), p1 + normal * (side * overshoot)});
        out.emplace_back(Line{d.b + normal * (side * gap), p2 + normal * (side * overshoot)});
    }

    // Text reads left to right (or bottom to top): flip it when the line points backwards.
    const bool reversed = dir.x < -core::kEpsilon || (std::abs(dir.x) <= core::kEpsilon && dir.y < 0.0);
    const Vec2 textDir = reversed ? -dir : dir;
    const Vec2 textUp(-textDir.y, textDir.x);
    const Real angle = std::atan2(textDir.y, textDir.x) * 180.0 / std::numbers::pi;

    Text label;
    label.text = d.text.empty() ? dimensionText(d) : d.text;
    label.height = h;
    label.angle = angle;
    label.vAlign = VAlign::Bottom;
    const Real textWidth = metrics.width(label.text, h, false);
    const Real arrow = d.arrowSize;

    if (length >= textWidth + 2.5 * arrow) {
        // Everything fits between the extension lines.
        out.emplace_back(Line{p1, p2});
        out.emplace_back(arrowHead(p1, -dir, arrow));
        out.emplace_back(arrowHead(p2, dir, arrow));
        label.position = (p1 + p2) * 0.5 + textUp * (0.3 * h);
        label.hAlign = HAlign::Center;
    } else {
        // Too short: arrows point in from outside, the text stands next to the second end.
        const Vec2 textEnd = reversed ? p1 : p2;
        const Vec2 outward = reversed ? -dir : dir; // from the middle towards textEnd
        const Vec2 farEnd = textEnd + outward * (arrow * 1.5 + textWidth + 0.5 * h);
        const Vec2 otherEnd = (reversed ? p2 : p1) - outward * (arrow * 1.5);
        out.emplace_back(Line{otherEnd, farEnd});
        out.emplace_back(arrowHead(p1, dir, arrow));
        out.emplace_back(arrowHead(p2, -dir, arrow));
        label.position = textEnd + outward * (arrow * 1.5 + 0.25 * h) + textUp * (0.3 * h);
        label.hAlign = (outward == textDir) ? HAlign::Left : HAlign::Right;
    }
    out.emplace_back(std::move(label));
    return out;
}

} // namespace rc::exporting
