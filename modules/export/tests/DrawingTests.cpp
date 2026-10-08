// Stage 6: the 2D drawing model shared by DXF and PDF.

#include "readycad/export/Drawing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <variant>

using namespace rc::exporting;
using Catch::Approx;

namespace {

const Layer kVisible{"VISIBLE", 7, {}, LineType::Continuous, LineWeight::Normal};
const Layer kHidden{"HIDDEN", 5, {}, LineType::Dashed, LineWeight::Thin};

template <typename T>
std::vector<T> shapesOf(const std::vector<Shape>& shapes)
{
    std::vector<T> result;
    for (const Shape& s : shapes) {
        if (const T* t = std::get_if<T>(&s)) {
            result.push_back(*t);
        }
    }
    return result;
}

} // namespace

TEST_CASE("Layers are unique by name", "[export][drawing]")
{
    Drawing d;
    const std::size_t a = d.addLayer(kVisible);
    const std::size_t b = d.addLayer(kHidden);
    CHECK(d.addLayer(kVisible) == a);
    CHECK(a != b);
    CHECK(d.layers().size() == 2);
    CHECK(d.layerIndex("HIDDEN") == b);
    CHECK(d.hasLayer("VISIBLE"));
    CHECK_FALSE(d.hasLayer("NOPE"));
    CHECK_THROWS(d.layerIndex("NOPE"));
    CHECK_THROWS(d.line(7, Vec2(0.0), Vec2(1.0))); // no such layer index
}

TEST_CASE("Appending scales and moves everything, layers are merged", "[export][drawing]")
{
    Drawing part;
    const std::size_t visible = part.addLayer(kVisible);
    part.rectangle(visible, Vec2(0.0), Vec2(100.0, 50.0));
    part.circle(visible, Vec2(10.0, 10.0), 4.0);
    part.text(visible, {.position = Vec2(0.0, 60.0), .text = "Raf", .height = 20.0});

    Drawing page;
    page.addLayer(kHidden);
    page.addLayer(kVisible);
    page.append(part, 0.1, Vec2(20.0, 30.0)); // 1:10, placed at (20, 30) mm

    REQUIRE(page.entities().size() == 3);
    CHECK(page.layers().size() == 2); // VISIBLE was not added twice
    for (const Entity& e : page.entities()) {
        CHECK(page.layers()[e.layer].name == "VISIBLE");
    }
    const auto& rect = std::get<Polyline>(page.entities()[0].shape);
    CHECK(rect.points[2] == Vec2(30.0, 35.0));
    const auto& circle = std::get<Circle>(page.entities()[1].shape);
    CHECK(circle.center == Vec2(21.0, 31.0));
    CHECK(circle.radius == Approx(0.4));
    const auto& text = std::get<Text>(page.entities()[2].shape);
    CHECK(text.height == Approx(2.0)); // texts scale with the drawing
    CHECK(text.position == Vec2(20.0, 36.0));
}

TEST_CASE("Appending with a quarter turn is exact, texts turn too", "[export][drawing]")
{
    Drawing part;
    const std::size_t visible = part.addLayer(kVisible);
    part.rectangle(visible, Vec2(0.0), Vec2(762.0, 518.0));
    part.text(visible, {.position = Vec2(10.0, 20.0), .text = "3", .height = 20.0});
    part.dimension(visible, {.a = Vec2(0.0), .b = Vec2(762.0, 0.0), .offset = -30.0});

    Drawing sheet;
    sheet.append(part, Transform2D{1.0, 90.0, Vec2(600.0, 10.0)}); // as a part turned on a sheet

    const auto& rect = std::get<Polyline>(sheet.entities()[0].shape);
    CHECK(rect.points[1] == Vec2(600.0, 772.0)); // (762, 0) -> (0, 762) + offset, no rounding
    CHECK(rect.points[2] == Vec2(82.0, 772.0));
    const auto& text = std::get<Text>(sheet.entities()[1].shape);
    CHECK(text.position == Vec2(580.0, 20.0));
    CHECK(text.angle == Approx(90.0));
    const auto& dim = std::get<Dimension>(sheet.entities()[2].shape);
    CHECK(dim.b == Vec2(600.0, 772.0));
    CHECK(dim.text == "762"); // frozen before turning

    CHECK(Transform2D{2.0, 180.0, Vec2(1.0, 1.0)}.apply(Vec2(3.0, 4.0)) == Vec2(-5.0, -7.0));
    CHECK(Transform2D{1.0, 45.0, Vec2(0.0)}.apply(Vec2(1.0, 0.0)).x == Approx(std::sqrt(0.5)));
}

TEST_CASE("Bounds include texts and dimensions", "[export][drawing]")
{
    Drawing d;
    const std::size_t layer = d.addLayer(kVisible);
    CHECK(d.bounds(ApproxTextMetrics{}).empty);

    d.rectangle(layer, Vec2(0.0), Vec2(100.0, 50.0));
    d.text(layer, {.position = Vec2(0.0, 60.0), .text = "Kapak", .height = 5.0});
    const Bounds withText = d.bounds(ApproxTextMetrics{});
    CHECK(withText.max.y >= 65.0);

    d.dimension(layer, {.a = Vec2(0.0, 0.0), .b = Vec2(100.0, 0.0), .offset = -15.0});
    const Bounds withDimension = d.bounds(ApproxTextMetrics{});
    CHECK(withDimension.min.y <= -15.0);
}

TEST_CASE("A dimension explodes into extension lines, arrows and its value", "[export][drawing]")
{
    const ApproxTextMetrics metrics;

    SECTION("long enough: arrows inside, value centred above the line")
    {
        const auto shapes = explode({Vec2(0.0), Vec2(762.0, 0.0), -30.0, "", 2.5, 2.5}, metrics);
        CHECK(shapesOf<Line>(shapes).size() == 3); // 2 extension lines + dimension line
        const auto arrows = shapesOf<Polyline>(shapes);
        REQUIRE(arrows.size() == 2);
        CHECK(arrows[0].filled);
        CHECK(arrows[0].points[0] == Vec2(0.0, -30.0)); // tips on the extension lines
        CHECK(arrows[1].points[0] == Vec2(762.0, -30.0));
        const auto texts = shapesOf<Text>(shapes);
        REQUIRE(texts.size() == 1);
        CHECK(texts[0].text == "762");
        CHECK(texts[0].hAlign == HAlign::Center);
        CHECK(texts[0].position.x == Approx(381.0));
        CHECK(texts[0].position.y > -30.0);
    }

    SECTION("vertical: the value reads from bottom to top, whatever the direction")
    {
        for (const auto& [a, b] : {std::pair(Vec2(0.0), Vec2(0.0, 2000.0)),
                                   std::pair(Vec2(0.0, 2000.0), Vec2(0.0))}) {
            const auto texts = shapesOf<Text>(explode({.a = a, .b = b, .offset = 20.0}, metrics));
            REQUIRE(texts.size() == 1);
            CHECK(texts[0].angle == Approx(90.0));
            CHECK(texts[0].text == "2000");
        }
    }

    SECTION("too short for the value: arrows point in from outside, value next to it")
    {
        const auto shapes = explode({Vec2(0.0), Vec2(18.0, 0.0), 10.0, "", 5.0, 5.0}, metrics);
        const auto arrows = shapesOf<Polyline>(shapes);
        REQUIRE(arrows.size() == 2);
        CHECK(arrows[0].points[1].x < 0.0); // the first arrow's body is left of its tip
        const auto texts = shapesOf<Text>(shapes);
        REQUIRE(texts.size() == 1);
        CHECK(texts[0].position.x > 18.0);
    }

    SECTION("explicit text and a zero length")
    {
        const auto texts = shapesOf<Text>(explode({.a = Vec2(0.0), .b = Vec2(300.0, 0.0), .offset = 10.0, .text = "3 × 100"}, metrics));
        CHECK(texts.at(0).text == "3 × 100");
        CHECK(explode({.a = Vec2(5.0), .b = Vec2(5.0), .offset = 10.0}, metrics).empty());
    }
}

TEST_CASE("Text alignment moves the baseline", "[export][drawing]")
{
    const ApproxTextMetrics metrics;
    Text t{.position = Vec2(100.0, 100.0), .text = "ABCD", .height = 10.0};
    const Real width = metrics.width("ABCD", 10.0, false);
    CHECK(baselineStart(t, metrics) == Vec2(100.0, 100.0));
    t.hAlign = HAlign::Center;
    t.vAlign = VAlign::Top;
    CHECK(baselineStart(t, metrics).x == Approx(100.0 - width / 2.0));
    CHECK(baselineStart(t, metrics).y == Approx(90.0)); // capitals hang below the point
    t.angle = 90.0;
    t.hAlign = HAlign::Left;
    t.vAlign = VAlign::Baseline;
    const auto corners = textCorners(t, metrics);
    CHECK(corners[1].y == Approx(100.0 + width)); // rotated: the text runs upwards
}

TEST_CASE("UTF-8 length counts letters, not bytes", "[export][drawing]")
{
    CHECK(utf8Length("Çıkış") == 5);
    CHECK(utf8Length("abc") == 3);
    CHECK(utf8Length("") == 0);
    const ApproxTextMetrics metrics;
    CHECK(metrics.width("Çıkış", 2.0, false) == Approx(metrics.width("Cikis", 2.0, false)));
    CHECK(metrics.width("ABC", 2.0, true) > metrics.width("ABC", 2.0, false));
}
