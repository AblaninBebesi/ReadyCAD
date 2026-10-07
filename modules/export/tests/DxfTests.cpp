// Stage 6: structure of the DXF files (sections, layers, primitives) and part export.

#include "DxfReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/Section.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <utility>

using namespace rc;
using namespace rc::exporting;
using rc::test::DxfObject;
using rc::test::dxfObjects;
using rc::test::dxfSections;
using rc::test::dxfTable;
using rc::test::parseDxf;
using Catch::Approx;
using rc::test::readFile;
using rc::test::TempFolder;

namespace {

const Layer kContour{"CONTOUR", 7, {}, LineType::Continuous, LineWeight::Thick};
const Layer kHoles{"DRILL_D5_Z13", 3, {}, LineType::Continuous, LineWeight::Normal};
const Layer kHidden{"HIDDEN", 5, {}, LineType::Dashed, LineWeight::Thin};

std::vector<DxfObject> ofType(const std::vector<DxfObject>& objects, const std::string& type)
{
    std::vector<DxfObject> result;
    std::copy_if(objects.begin(), objects.end(), std::back_inserter(result),
                 [&](const DxfObject& o) { return o.type == type; });
    return result;
}

std::set<std::string> layerNames(const std::vector<test::DxfPair>& pairs)
{
    std::set<std::string> names;
    for (const DxfObject& layer : dxfTable(pairs, "LAYER")) {
        names.insert(layer.get(2));
    }
    return names;
}

} // namespace

TEST_CASE("DXF file has the R12 structure", "[export][dxf]")
{
    Drawing d;
    d.line(d.addLayer(kContour), Vec2(0.0), Vec2(100.0, 50.0));
    const std::string dxf = toDxf(d);
    const auto pairs = parseDxf(dxf); // every group code is a number, lines come in pairs

    CHECK(dxfSections(pairs) == std::vector<std::string>{"HEADER", "TABLES", "BLOCKS", "ENTITIES"});
    REQUIRE(pairs.size() >= 2);
    CHECK(pairs.back().code == 0);
    CHECK(pairs.back().value == "EOF");

    // Version and extents in the header (variables: code 9 name, then its values).
    std::map<std::string, std::string> variables;
    std::string name;
    bool inHeader = false;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        if (pairs[i].code == 2 && pairs[i].value == "HEADER") {
            inHeader = true;
        } else if (inHeader && pairs[i].code == 0) {
            break;
        } else if (inHeader && pairs[i].code == 9) {
            name = pairs[i].value;
        } else if (inHeader && !name.empty() && !variables.contains(name)) {
            variables[name] = pairs[i].value;
        }
    }
    CHECK(variables["$ACADVER"] == "AC1009");
    CHECK(variables["$EXTMAX"] == "100");

    // Line types used by layers are defined.
    std::set<std::string> lineTypes;
    for (const DxfObject& ltype : dxfTable(pairs, "LTYPE")) {
        lineTypes.insert(ltype.get(2));
    }
    CHECK(lineTypes == std::set<std::string>{"CONTINUOUS", "DASHED", "CENTER"});
}

TEST_CASE("Every layer is declared with its colour and line type", "[export][dxf]")
{
    Drawing d;
    const std::size_t contour = d.addLayer(kContour);
    const std::size_t holes = d.addLayer(kHoles);
    const std::size_t hidden = d.addLayer(kHidden);
    d.rectangle(contour, Vec2(0.0), Vec2(762.0, 518.0));
    d.circle(holes, Vec2(37.0, 37.0), 2.5);
    d.circle(hidden, Vec2(100.0, 37.0), 2.5);
    const auto pairs = parseDxf(toDxf(d));

    const auto layers = dxfTable(pairs, "LAYER");
    CHECK(layerNames(pairs) == std::set<std::string>{"0", "CONTOUR", "DRILL_D5_Z13", "HIDDEN"});
    for (const DxfObject& layer : layers) {
        if (layer.get(2) == "HIDDEN") {
            CHECK(layer.get(62) == "5");
            CHECK(layer.get(6) == "DASHED");
        }
        if (layer.get(2) == "DRILL_D5_Z13") {
            CHECK(layer.get(62) == "3");
            CHECK(layer.get(6) == "CONTINUOUS");
        }
    }

    // Each entity refers to a declared layer.
    const std::set<std::string> declared = layerNames(pairs);
    for (const DxfObject& e : dxfObjects(pairs, "ENTITIES")) {
        INFO(e.type);
        CHECK(declared.contains(e.get(8)));
    }
}

TEST_CASE("Primitives are written as DXF entities", "[export][dxf]")
{
    Drawing d;
    const std::size_t layer = d.addLayer(kContour);
    d.line(layer, Vec2(0.0), Vec2(10.0, 0.5));
    d.circle(layer, Vec2(37.0, 9.0), 2.5);
    d.rectangle(layer, Vec2(0.0), Vec2(762.0, 518.0));
    d.add(layer, Polyline{{Vec2(0.0), Vec2(5.0, 1.0), Vec2(5.0, -1.0)}, true, true}); // arrow
    d.text(layer, {Vec2(10.0, 20.0), "Çekmece yanı ⌀8", 3.5, 90.0, HAlign::Center,
                   VAlign::Middle, false});

    const auto entities = dxfObjects(parseDxf(toDxf(d)), "ENTITIES");

    const auto lines = ofType(entities, "LINE");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].number(11) == Approx(10.0));
    CHECK(lines[0].get(21) == "0.5"); // decimal point whatever the locale, no trailing zeros

    const auto circles = ofType(entities, "CIRCLE");
    REQUIRE(circles.size() == 1);
    CHECK(circles[0].number(10) == Approx(37.0));
    CHECK(circles[0].number(40) == Approx(2.5));

    // POLYLINE, 4 VERTEX, SEQEND; closed flag 1.
    const auto polylines = ofType(entities, "POLYLINE");
    REQUIRE(polylines.size() == 1);
    CHECK(polylines[0].get(70) == "1");
    CHECK(polylines[0].get(66) == "1");
    CHECK(ofType(entities, "VERTEX").size() == 4);
    CHECK(ofType(entities, "SEQEND").size() == 1);

    // A filled triangle (arrow head) is a SOLID with its third point repeated.
    const auto solids = ofType(entities, "SOLID");
    REQUIRE(solids.size() == 1);
    CHECK(solids[0].get(12) == solids[0].get(13));

    // Turkish letters as \U+XXXX, the diameter sign as %%c, alignment groups present.
    const auto texts = ofType(entities, "TEXT");
    REQUIRE(texts.size() == 1);
    CHECK(texts[0].get(1) == "\\U+00C7ekmece yan\\U+0131 %%c8");
    CHECK(texts[0].get(40) == "3.5");
    CHECK(texts[0].get(50) == "90");
    CHECK(texts[0].get(72) == "1");
    CHECK(texts[0].get(73) == "2");
    CHECK(texts[0].number(11) == Approx(10.0));
}

TEST_CASE("Dimensions are exploded into plain entities", "[export][dxf]")
{
    Drawing d;
    d.dimension(d.addLayer(kContour), {Vec2(0.0), Vec2(762.0, 0.0), -30.0, "", 25.0, 25.0});
    const auto entities = dxfObjects(parseDxf(toDxf(d)), "ENTITIES");
    CHECK(ofType(entities, "LINE").size() == 3);
    CHECK(ofType(entities, "SOLID").size() == 2);
    const auto texts = ofType(entities, "TEXT");
    REQUIRE(texts.size() == 1);
    CHECK(texts[0].get(1) == "762");
}

TEST_CASE("Names are made safe for DXF and CNC file systems", "[export][dxf]")
{
    CHECK(asciiFold("Çekmece yanı, Şifonyer, İğne, Ölçü") == "Cekmece yani, Sifonyer, Igne, Olcu");
    CHECK(dxfName("Raf 2") == "RAF_2");
    CHECK(dxfName("Çekmece yanı") == "CEKMECE_YANI");
    CHECK(dxfName("") == "0");
    CHECK(dxfName(std::string(40, 'A')).size() == 31);
}

// ---------------------------------------------------------------------------------------------
// Parts
// ---------------------------------------------------------------------------------------------

namespace {

std::shared_ptr<core::Product> detailedWardrobe()
{
    auto wardrobe = core::WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    detailing::applyDetailing(*wardrobe);
    return wardrobe;
}

} // namespace

TEST_CASE("Layer names encode tool and depth", "[export][dxf][parts]")
{
    using detailing::FlatSide;
    CHECK(drillLayerName(FlatSide::Near, 5.0, 13.0) == "DRILL_D5_Z13");
    CHECK(drillLayerName(FlatSide::Far, 15.0, 12.5) == "DRILL_BACK_D15_Z12p5");
    CHECK(drillLayerName(FlatSide::Edge, 8.0, 30.0) == "DRILL_EDGE_D8_Z30");
    CHECK(grooveLayerName(FlatSide::Near, 4.0, 8.0) == "GROOVE_W4_Z8");
}

TEST_CASE("A side wall in DXF: contour, holes per layer, edge bands", "[export][dxf][parts]")
{
    const auto wardrobe = detailedWardrobe();
    const core::Panel& wall = *wardrobe->carcass()->leftSide().panel();
    const detailing::FlatPart flat = detailing::flatten(wall, wardrobe->rules());

    PartDrawingOptions options;
    options.cutContour = true;
    const Drawing drawing = drawPart(flat, options);
    const auto pairs = parseDxf(toDxf(drawing));
    const auto entities = dxfObjects(pairs, "ENTITIES");

    // Contour: one closed polyline with the finish size.
    CHECK(drawing.count<Polyline>(layers::kContour) == 1);
    CHECK(drawing.count<Polyline>(layers::kCutContour) == 1);
    const auto& contour = std::get<Polyline>(drawing.entities().front().shape);
    CHECK(contour.closed);
    CHECK(contour.points[2] == Vec2(wall.finishLength(), wall.finishWidth()));

    // Every near/far face hole is a circle on the layer of its diameter and depth ...
    std::size_t faceHoles = 0;
    for (const detailing::FlatHole& hole : flat.holes) {
        if (hole.side != detailing::FlatSide::Edge) {
            ++faceHoles;
            CHECK(drawing.hasLayer(drillLayerName(hole.side, hole.diameter, hole.depth)));
        }
    }
    CHECK(drawing.count<Circle>() == faceHoles + 1); // + the origin mark on TEXT
    std::size_t dxfCircles = 0;
    for (const DxfObject& e : entities) {
        if (e.type == "CIRCLE" && e.get(8).starts_with("DRILL_")) {
            ++dxfCircles;
        }
    }
    CHECK(dxfCircles == faceHoles);

    // ... and the declared layers include them.
    const std::set<std::string> declared = layerNames(pairs);
    CHECK(declared.contains("CONTOUR"));
    CHECK(declared.contains("EDGE_BAND"));
    CHECK(declared.contains("DIMENSIONS"));
    CHECK(std::any_of(declared.begin(), declared.end(),
                      [](const std::string& n) { return n.starts_with("DRILL_D5_"); }));

    // The front edge of the wall carries the visible 2 mm band: one mark.
    std::size_t banded = 0;
    for (const detailing::FlatEdge& e : flat.outline) {
        banded += e.band.present() ? 1 : 0;
    }
    CHECK(drawing.count<Line>(layers::kEdgeBand) == banded);
    CHECK(banded >= 1);
}

TEST_CASE("All parts in one DXF: one contour per cut list row, no overlaps", "[export][dxf][parts]")
{
    const auto wardrobe = detailedWardrobe();
    const auto cutList = detailing::buildCutList(*wardrobe, wardrobe->rules());
    const Drawing sheet = drawAllParts(*wardrobe, cutList, {20.0, 150.0, 3000.0, true});
    CHECK(sheet.count<Polyline>(layers::kContour) == cutList.size());

    std::vector<Bounds> boxes;
    for (const Entity& e : sheet.entities()) {
        if (sheet.layers()[e.layer].name == layers::kContour) {
            Bounds b;
            for (const Vec2& p : std::get<Polyline>(e.shape).points) {
                b.add(p);
            }
            boxes.push_back(b);
        }
    }
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            const bool apart = boxes[i].max.x < boxes[j].min.x || boxes[j].max.x < boxes[i].min.x
                            || boxes[i].max.y < boxes[j].min.y || boxes[j].max.y < boxes[i].min.y;
            CHECK(apart);
        }
    }
    // Wrapped into rows: never much wider than asked.
    CHECK(sheet.bounds(ApproxTextMetrics{}).size().x < 3000.0 + 2500.0);
    CHECK(parseDxf(toDxf(sheet)).size() > 100);
}

TEST_CASE("One DXF file per part, with readable ASCII names", "[export][dxf][parts]")
{
    const auto wardrobe = detailedWardrobe();
    const auto cutList = detailing::buildCutList(*wardrobe, wardrobe->rules());
    CHECK(partFileName(3, cutList.front()).starts_with("03_Yan_panel_"));

    const TempFolder folder("part_dxfs");
    const auto files = savePartDxfs(*wardrobe, cutList, folder.path() / "nested");
    REQUIRE(files.size() == cutList.size());
    for (const std::filesystem::path& file : files) {
        INFO(file.string());
        REQUIRE(std::filesystem::exists(file));
        CHECK(file.extension() == ".dxf");
        const auto pairs = parseDxf(readFile(file));
        CHECK(layerNames(pairs).contains("CONTOUR"));
        CHECK(pairs.back().value == "EOF");
    }
}

TEST_CASE("Writing to an impossible path reports an error", "[export][dxf]")
{
    Drawing d;
    d.line(d.addLayer(kContour), Vec2(0.0), Vec2(1.0));
    const TempFolder folder("bad_path");
    std::filesystem::create_directories(folder.path() / "file.dxf"); // a folder with that name
    CHECK_THROWS_AS(saveDxf(d, folder.path() / "file.dxf"), ExportError);
}
