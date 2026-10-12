// Stage 7: sheet layouts as drawings and DXF files — every part where the optimiser put it,
// turned with its holes, remnants and saw cuts on their own layers.

#include "readycad/test/DxfReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/Pdf.hpp"
#include "readycad/export/SheetDrawing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>

using namespace rc;
using namespace rc::exporting;
using Catch::Approx;

namespace {

struct Job {
    std::shared_ptr<core::Product> product;
    std::vector<detailing::CutListRow> cutList;
    nesting::NestingResult result;

    explicit Job(nesting::Machine machine)
        : product(core::createProduct(core::CatalogItem::Wardrobe))
    {
        detailing::applyDetailing(*product);
        cutList = detailing::buildCutList(*product, product->rules());
        nesting::NestingSettings settings;
        settings.machine = machine;
        settings.optimizer.generations = 3;
        settings.optimizer.populationSize = 8;
        result = nesting::optimize(nesting::partsFromCutList(*product, cutList), {}, settings);
    }
    [[nodiscard]] PartSource source() const { return {*product, cutList}; }
};

std::set<std::string> layerNames(const Drawing& d)
{
    std::set<std::string> names;
    for (const Layer& l : d.layers()) {
        names.insert(l.name);
    }
    return names;
}

} // namespace

TEST_CASE("A sheet layout draws every part at its place", "[export][sheet]")
{
    for (const nesting::Machine machine :
         {nesting::Machine::PanelSaw, nesting::Machine::NestingCenter}) {
        const Job job(machine);
        const PartSource source = job.source();
        REQUIRE_FALSE(job.result.layouts.empty());
        for (std::size_t i = 0; i < job.result.layouts.size(); ++i) {
            const nesting::SheetLayout& layout = job.result.layouts[i];
            const Drawing d = drawSheetLayout(job.result, i, &source);
            CHECK(d.count<Polyline>(layers::kSheet) == 1);
            CHECK(d.count<Polyline>(layers::kContour) == layout.parts.size());
            CHECK(d.count<Polyline>(layers::kCutContour) == layout.parts.size());
            CHECK(d.count<Polyline>(layers::kRemnant) == layout.remnants.size());
            CHECK(d.count<Line>(layers::kSawCut) == layout.cuts.size());
            if (machine == nesting::Machine::NestingCenter) {
                CHECK(layout.cuts.empty());
            }

            // The sawn blank of every part lies exactly where the result says.
            std::vector<nesting::Rect> blanks;
            for (const Entity& e : d.entities()) {
                if (d.layers()[e.layer].name == layers::kCutContour) {
                    nesting::Polygon points = std::get<Polyline>(e.shape).points;
                    blanks.push_back(nesting::bounds(points));
                }
            }
            for (const nesting::PlacedPart& p : layout.parts) {
                const nesting::Rect expected = nesting::bounds(
                    nesting::transformed(job.result.parts[p.part].outline, p.transform));
                const auto near = [](Vec2 a, Vec2 b) {
                    return std::abs(a.x - b.x) < 1e-6 && std::abs(a.y - b.y) < 1e-6;
                };
                const bool found = std::any_of(blanks.begin(), blanks.end(), [&](const auto& b) {
                    return near(b.min, expected.min) && near(b.max, expected.max);
                });
                CHECK(found);
            }
        }
    }
}

TEST_CASE("A cutting map for people: outlines, shading and labels", "[export][sheet]")
{
    const Job job(nesting::Machine::PanelSaw);
    const PartSource source = job.source();
    SheetDrawingOptions options;
    options.partDetails = false;
    options.fills = true;
    options.textHeight = 40.0;
    const Drawing d = drawSheetLayout(job.result, 0, &source, options);
    const nesting::SheetLayout& layout = job.result.layouts.front();
    CHECK(d.count<Polyline>(layers::kPartFill) == layout.parts.size());
    CHECK(d.count<Polyline>(layers::kCutContour) == layout.parts.size());
    CHECK(d.count<Polyline>(layers::kContour) == 0); // no part details
    CHECK_FALSE(layerNames(d).contains("DRILL_D5_Z13"));
    // Every part carries its cut list number.
    std::set<std::string> texts;
    for (const Entity& e : d.entities()) {
        if (const auto* t = std::get_if<Text>(&e.shape)) {
            texts.insert(t->text);
        }
    }
    for (const nesting::PlacedPart& p : layout.parts) {
        CHECK(texts.contains(job.result.parts[p.part].label));
    }
}

TEST_CASE("Shaped parts without a cut list are drawn by their outline", "[export][sheet]")
{
    nesting::NestingPart triangle;
    triangle.label = "T";
    triangle.outline = {{0, 0}, {700, 0}, {200, 450}};
    triangle.quantity = 3;
    nesting::NestingSettings settings;
    settings.machine = nesting::Machine::NestingCenter;
    settings.optimizer.generations = 0;
    const nesting::NestingResult r = nesting::optimize({triangle}, {}, settings);
    const Drawing d = drawSheetLayout(r, 0, nullptr);
    CHECK(d.count<Polyline>(layers::kCutContour) == 3);
    CHECK(placedPartName(r, 0, nullptr) == "T");
}

TEST_CASE("One DXF per sheet layout, with readable names", "[export][sheet][dxf]")
{
    const Job job(nesting::Machine::PanelSaw);
    const PartSource source = job.source();
    const test::TempFolder folder("sheet_dxfs");
    const auto files = saveSheetDxfs(job.result, &source, folder.path());
    REQUIRE(files.size() == job.result.layouts.size());
    for (std::size_t i = 0; i < files.size(); ++i) {
        CHECK(files[i].filename().string() == sheetFileName(job.result, i));
        const auto pairs = test::parseDxf(test::readFile(files[i]));
        std::set<std::string> layerSet;
        for (const test::DxfObject& layer : test::dxfTable(pairs, "LAYER")) {
            layerSet.insert(layer.get(2));
        }
        CHECK(layerSet.contains("SHEET"));
        CHECK(layerSet.contains("CUT_CONTOUR"));
        CHECK(layerSet.contains("SAW_CUT"));
    }
    CHECK(sheetFileName(job.result, 0).starts_with("01_"));
    CHECK(sheetFileName(job.result, 0).ends_with(".dxf"));

    nesting::NestingResult twice = job.result;
    twice.layouts.front().count = 2;
    twice.layouts.front().sheet.material.name = "Suntalam 18 Meşe";
    CHECK(sheetFileName(twice, 0) == "01_Suntalam_18_Mese_2800x2100_x2.dxf");
    CHECK(sheetTitle(twice, 0).find("×2") != std::string::npos);
}

// Not a check: run by hand ("[demo]") to make the picture of shaped nesting for docs/images.
TEST_CASE("Demo: shaped parts on a nesting centre, as a PDF page", "[.][demo]")
{
    const auto shape = [](const std::string& label, nesting::Polygon outline, int quantity) {
        nesting::NestingPart p;
        p.label = label;
        p.material = core::Material{"MDF 18", core::MaterialType::MDF, 18.0, false};
        p.outline = std::move(outline);
        p.quantity = quantity;
        return p;
    };
    const std::vector<nesting::NestingPart> parts{
        shape("L", {{0, 0}, {600, 0}, {600, 250}, {250, 250}, {250, 600}, {0, 600}}, 4),
        shape("T", {{0, 0}, {700, 0}, {200, 450}}, 5),
        shape("U", {{0, 0}, {800, 0}, {800, 500}, {600, 500}, {600, 200}, {200, 200}, {200, 500},
                    {0, 500}},
              2),
        shape("H", {{150, 0}, {450, 0}, {600, 260}, {450, 520}, {150, 520}, {0, 260}}, 3),
        shape("B", nesting::rectangle(nesting::Rect{{0, 0}, {350, 150}}), 6),
        shape("R", nesting::rectangle(nesting::Rect{{0, 0}, {560, 300}}), 3),
    };
    nesting::NestingSettings settings;
    settings.machine = nesting::Machine::NestingCenter;
    settings.optimizer.generations = 60;
    settings.optimizer.populationSize = 30;
    const nesting::NestingResult r = nesting::optimize(parts, {}, settings);

    SheetDrawingOptions options;
    options.partDetails = false;
    options.fills = true;
    options.textHeight = 2.4 * 15.0;
    PageSet set;
    set.title = "Nesting demo";
    for (std::size_t i = 0; i < r.layouts.size(); ++i) {
        Page page{Vec2(297.0, 210.0), {}, {}};
        options.title = sheetTitle(r, i);
        page.drawing.append(drawSheetLayout(r, i, nullptr, options), 1.0 / 15.0, Vec2(15.0, 30.0));
        set.pages.push_back(std::move(page));
    }
    const auto folder = std::filesystem::temp_directory_path() / "readycad_demo";
    std::filesystem::create_directories(folder);
    savePdf(set, folder / "shapes.pdf", findPdfFonts());
    SUCCEED("written to " << (folder / "shapes.pdf").string());
}
