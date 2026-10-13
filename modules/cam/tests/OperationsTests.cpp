// Stage 8: machining operations, machine-independent: holes in faces and edges, grooves,
// contours; moved with a part onto a sheet.

#include "readycad/cam/Operations.hpp"
#include "readycad/core/builders/Catalog.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace rc;
using namespace rc::cam;
using Catch::Approx;

namespace {

struct Wardrobe {
    std::shared_ptr<core::Product> product = core::createProduct(core::CatalogItem::Wardrobe);
    std::vector<detailing::CutListRow> cutList;

    Wardrobe()
    {
        detailing::applyDetailing(*product);
        cutList = detailing::buildCutList(*product, product->rules());
    }
};

template <typename T>
std::vector<T> all(const PartMachining& part)
{
    std::vector<T> out;
    for (const Operation& op : part.operations) {
        if (const auto* t = std::get_if<T>(&op)) {
            out.push_back(*t);
        }
    }
    return out;
}

} // namespace

TEST_CASE("Faces and their directions", "[cam][operations]")
{
    CHECK(inward(Face::Top) == Vec3(0, 0, -1));
    CHECK(inward(Face::Bottom) == Vec3(0, 0, 1));
    CHECK(inward(Face::Left) == Vec3(1, 0, 0));
    CHECK(inward(Face::Back) == Vec3(0, -1, 0));
    for (const Face f : {Face::Left, Face::Right, Face::Front, Face::Back}) {
        CHECK(edgeFace(planNormal(f)) == f);
    }
}

TEST_CASE("Every hole, groove and the contour of a part are operations", "[cam][operations]")
{
    const Wardrobe w;
    for (const detailing::CutListRow& row : w.cutList) {
        const detailing::FlatPart flat = detailing::flatPartOf(*w.product, row);
        const PartMachining m = partMachining(
            flat, nesting::rectangle(nesting::Rect{flat.cut.min, flat.cut.max}), "1", "x");
        const auto holes = all<Drilling>(m);
        CHECK(holes.size() == flat.holes.size());
        CHECK(all<Groove>(m).size() == flat.grooves.size());
        REQUIRE(all<Contour>(m).size() == 1);
        CHECK(nesting::bounds(all<Contour>(m).front().outline).min == flat.cut.min);
        CHECK(std::count_if(holes.begin(), holes.end(), [](const Drilling& d) {
                  return d.face == Face::Top;
              }) == static_cast<long>(flat.count(detailing::FlatSide::Near)));
        for (const Drilling& d : holes) {
            // Every hole enters on its face and goes into the part.
            switch (d.face) {
            case Face::Top:    CHECK(d.entry.z == Approx(flat.thickness)); break;
            case Face::Bottom: CHECK(d.entry.z == Approx(0.0)); break;
            case Face::Left:   CHECK(d.entry.x == Approx(0.0).margin(1e-9)); break;
            case Face::Right:  CHECK(d.entry.x == Approx(flat.length)); break;
            case Face::Front:  CHECK(d.entry.y == Approx(0.0).margin(1e-9)); break;
            case Face::Back:   CHECK(d.entry.y == Approx(flat.width)); break;
            }
            if (d.face != Face::Top && d.face != Face::Bottom) {
                CHECK(d.entry.z > 0.0);
                CHECK(d.entry.z < flat.thickness);
            }
        }
    }
}

TEST_CASE("Operations turn with the part", "[cam][operations]")
{
    const nesting::Transform turn{1, {700.0, 10.0}}; // 90° counter-clockwise, then moved
    const Operation edge = transformed(Drilling{Face::Left, {0.0, 100.0, 9.0}, 8.0, 30.0}, turn);
    const auto& d = std::get<Drilling>(edge);
    CHECK(d.face == Face::Front); // the left edge now looks down (-y)
    CHECK(d.entry == Vec3(600.0, 10.0, 9.0));

    const Operation top = transformed(Drilling{Face::Top, {50.0, 37.0, 18.0}, 5.0, 13.0}, turn);
    CHECK(std::get<Drilling>(top).face == Face::Top);
    CHECK(std::get<Drilling>(top).entry == Vec3(663.0, 60.0, 18.0));

    const Operation groove = transformed(Groove{Face::Top, {0, 16}, {500, 16}, 4.0, 8.0}, turn);
    CHECK(std::get<Groove>(groove).end == Vec2(684.0, 510.0));
}

TEST_CASE("A sheet of a cutting plan becomes a job with its parts in place", "[cam][operations]")
{
    const Wardrobe w;
    nesting::NestingSettings settings;
    settings.machine = nesting::Machine::NestingCenter;
    settings.optimizer.generations = 2;
    settings.optimizer.populationSize = 6;
    const nesting::NestingResult r =
        nesting::optimize(nesting::partsFromCutList(*w.product, w.cutList), {}, settings);
    const detailing::PartSource source{*w.product, w.cutList};
    for (std::size_t i = 0; i < r.layouts.size(); ++i) {
        const SheetJob job = sheetJob(r, i, &source);
        const nesting::SheetLayout& layout = r.layouts[i];
        CHECK(job.length == layout.sheet.length);
        CHECK(job.thickness == layout.sheet.material.thickness);
        CHECK(job.partSpacing == settings.router.spacing());
        CHECK(job.name.starts_with((i < 9 ? "0" : "") + std::to_string(i + 1) + "_"));
        REQUIRE(job.parts.size() == layout.parts.size());
        for (std::size_t k = 0; k < job.parts.size(); ++k) {
            const auto contours = all<Contour>(job.parts[k]);
            REQUIRE(contours.size() == 1);
            const nesting::Rect expected = nesting::bounds(nesting::transformed(
                r.parts[layout.parts[k].part].outline, layout.parts[k].transform));
            CHECK(nesting::bounds(contours.front().outline) == expected);
            // Holes of the face lying up stay inside the part.
            for (const Drilling& d : all<Drilling>(job.parts[k])) {
                CHECK(d.entry.x >= expected.min.x - 1e-6);
                CHECK(d.entry.x <= expected.max.x + 1e-6);
            }
        }
    }
    // Without a cut list only the contours are known.
    const SheetJob bare = sheetJob(r, 0, nullptr);
    for (const PartMachining& p : bare.parts) {
        CHECK(p.operations.size() == 1);
    }
}

TEST_CASE("The openings of a part come before its outline", "[cam][operations][kitchen]")
{
    detailing::FlatPart part;
    part.length = 2400.0;
    part.width = 600.0;
    part.thickness = 38.0;
    part.cut = {Vec2(0.0), Vec2(2400.0, 600.0)};
    part.cutouts = {{Vec2(700.0, 300.0), 560.0, 480.0, 15.0}, {Vec2(1700.0, 305.0), 560.0, 490.0, 5.0}};
    const nesting::Polygon blank = nesting::rectangle(nesting::Rect{part.cut.min, part.cut.max});
    const PartMachining m = partMachining(part, blank, "1", "Tezgah");
    REQUIRE(m.operations.size() == 3);
    const auto& sink = std::get<Contour>(m.operations[0]);
    CHECK(sink.inner);
    CHECK(nesting::bounds(sink.outline).width() == Approx(560.0));
    CHECK(std::get<Contour>(m.operations[1]).inner);
    CHECK_FALSE(std::get<Contour>(m.operations[2]).inner);
}
