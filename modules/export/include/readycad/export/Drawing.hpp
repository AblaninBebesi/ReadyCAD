#pragma once

#include "readycad/core/Math.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// Stage 6: drawings, DXF and PDF documentation.
///
/// The namespace is `exporting` because `export` is a C++ keyword.
namespace rc::exporting {

using core::Real;
using core::Vec2;

// ---------------------------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------------------------

enum class LineType { Continuous, Dashed, Center };

/// Pen width on paper: 0.18 / 0.25 / 0.5 mm (ISO 128 line groups). DXF R12 has no line
/// weights, there the colour tells the lines apart.
enum class LineWeight { Thin, Normal, Thick };

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

/// Every entity sits on a layer that decides how it looks. In DXF the layer name is also the
/// contract with CAD/CAM software ("all circles on DRILL_D5_Z13 are 5 mm holes 13 deep"), so
/// names use only A-Z, 0-9 and _ (what DXF R12 allows).
struct Layer {
    std::string name;
    int aci = 7;       ///< DXF colour (AutoCAD Colour Index): 7 = black/white
    Rgb print;         ///< colour in PDF
    LineType lineType = LineType::Continuous;
    LineWeight weight = LineWeight::Normal;
};

// ---------------------------------------------------------------------------------------------
// Entities (all coordinates in mm of the drawing, y up)
// ---------------------------------------------------------------------------------------------

struct Line {
    Vec2 a{0.0};
    Vec2 b{0.0};
};

struct Polyline {
    std::vector<Vec2> points;
    bool closed = false;
    bool filled = false; ///< solid fill (arrow heads); DXF writes 3-4 point fills as SOLID
};

struct Circle {
    Vec2 center{0.0};
    Real radius = 0.0;
};

enum class HAlign { Left, Center, Right };
enum class VAlign { Baseline, Bottom, Middle, Top };

/// Single-line text. `height` is the height of capital letters (as in DXF), not the font size.
struct Text {
    Vec2 position{0.0};
    std::string text; ///< UTF-8
    Real height = 2.5;
    Real angle = 0.0; ///< degrees, counter-clockwise
    HAlign hAlign = HAlign::Left;
    VAlign vAlign = VAlign::Baseline;
    bool bold = false;
};

/// Linear dimension between two points, measured along a -> b. The dimension line runs parallel
/// to a -> b at `offset` from it (positive = to the left when walking from a to b).
/// Backends draw it from lines, arrows and text (see explode()), so DXF needs no DIMSTYLE.
struct Dimension {
    Vec2 a{0.0};
    Vec2 b{0.0};
    Real offset = 10.0;
    std::string text{};     ///< empty: the measured length in mm
    Real textHeight = 2.5;
    Real arrowSize = 2.5;
};

using Shape = std::variant<Line, Polyline, Circle, Text, Dimension>;

struct Entity {
    std::size_t layer = 0; ///< index into Drawing::layers()
    Shape shape;
};

// ---------------------------------------------------------------------------------------------
// Text measuring
// ---------------------------------------------------------------------------------------------

/// Width of a text line, needed to fit tables and to centre texts. The PDF writer measures with
/// the real font; elsewhere an estimate is enough.
class TextMetrics {
public:
    virtual ~TextMetrics() = default;
    /// Width of `text` (UTF-8) whose capital letters are `height` tall, same units.
    [[nodiscard]] virtual Real width(std::string_view text, Real height, bool bold) const = 0;
};

/// Average-width estimate for a sans-serif font, slightly generous so that text fits.
class ApproxTextMetrics final : public TextMetrics {
public:
    [[nodiscard]] Real width(std::string_view text, Real height, bool bold) const override;
};

/// Number of Unicode code points in a UTF-8 string.
[[nodiscard]] std::size_t utf8Length(std::string_view text) noexcept;

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

/// Scale, then turn counter-clockwise about the origin, then move:
/// p' = rotate(p * scale, angle) + offset. Multiples of 90° are exact.
struct Transform2D {
    Real scale = 1.0;
    Real angle = 0.0; ///< degrees, counter-clockwise
    Vec2 offset{0.0};

    [[nodiscard]] Vec2 apply(Vec2 p) const noexcept;
};

struct Bounds {
    Vec2 min{0.0};
    Vec2 max{0.0};
    bool empty = true;

    void add(Vec2 p) noexcept;
    void add(const Bounds& other) noexcept;
    [[nodiscard]] Vec2 size() const noexcept { return empty ? Vec2(0.0) : max - min; }
    [[nodiscard]] Vec2 center() const noexcept { return (min + max) * 0.5; }
};

/// A 2D vector drawing: layers + entities. Built once and written by any backend — DXF for
/// CAD/CAM, PDF pages for people. Part drawings, product views, report pages are all Drawings.
class Drawing {
public:
    /// Adds the layer, or returns the existing one with the same name (its style is kept).
    std::size_t addLayer(const Layer& layer);
    /// Index of a layer by name; throws std::out_of_range if there is none.
    [[nodiscard]] std::size_t layerIndex(std::string_view name) const;
    [[nodiscard]] bool hasLayer(std::string_view name) const noexcept;

    [[nodiscard]] const std::vector<Layer>& layers() const noexcept { return layers_; }
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept { return entities_; }
    [[nodiscard]] bool isEmpty() const noexcept { return entities_.empty(); }

    void add(std::size_t layer, Shape shape);
    void line(std::size_t layer, Vec2 a, Vec2 b) { add(layer, Line{a, b}); }
    void rectangle(std::size_t layer, Vec2 min, Vec2 max, bool filled = false);
    void circle(std::size_t layer, Vec2 center, Real radius) { add(layer, Circle{center, radius}); }
    void text(std::size_t layer, Text text) { add(layer, std::move(text)); }
    void dimension(std::size_t layer, Dimension dimension) { add(layer, std::move(dimension)); }

    /// Copies `other` into this drawing as p' = p * scale + offset (texts and dimensions are
    /// scaled too, dimensions keep showing their real length). Layers are merged by name.
    void append(const Drawing& other, Real scale = 1.0, Vec2 offset = Vec2(0.0))
    {
        append(other, Transform2D{scale, 0.0, offset});
    }
    /// The same with a rotation: texts turn with the drawing (a part turned on a sheet).
    void append(const Drawing& other, const Transform2D& transform);

    /// Extent of everything, texts included (estimated with `metrics`).
    [[nodiscard]] Bounds bounds(const TextMetrics& metrics) const;

    /// Number of entities of a kind, optionally only on one layer — mostly for tests.
    template <typename T>
    [[nodiscard]] std::size_t count(std::string_view layer = {}) const
    {
        std::size_t n = 0;
        for (const Entity& e : entities_) {
            if (std::holds_alternative<T>(e.shape)
                && (layer.empty() || layers_[e.layer].name == layer)) {
                ++n;
            }
        }
        return n;
    }

private:
    std::vector<Layer> layers_;
    std::vector<Entity> entities_;
};

/// The lines, arrow heads and text that draw a dimension.
[[nodiscard]] std::vector<Shape> explode(const Dimension& dimension, const TextMetrics& metrics);

/// The four corners of a text's box (for bounds and tests).
[[nodiscard]] std::vector<Vec2> textCorners(const Text& text, const TextMetrics& metrics);

/// Where the baseline of a text starts once its alignment is applied (what PDF needs).
[[nodiscard]] Vec2 baselineStart(const Text& text, const TextMetrics& metrics);

/// Default dimension text: the length in mm without useless decimals ("762", "559.5").
[[nodiscard]] std::string dimensionText(const Dimension& dimension);

} // namespace rc::exporting
