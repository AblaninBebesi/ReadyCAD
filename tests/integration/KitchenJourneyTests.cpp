// Kitchen stage, end to end: a kitchen from the catalogue made to measure, saved and opened
// again, its worktop cut on the nesting centre with the other parts — the openings for the sink
// and the hob in the CNC program — through the same functions the window calls.

#include "readycad/core/model/Queries.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/production/Pipeline.hpp"
#include "readycad/storage/Storage.hpp"
#include "readycad/test/DxfReader.hpp"
#include "readycad/test/GCodeReader.hpp"
#include "readycad/test/PdfReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

using namespace rc;

TEST_CASE("Kitchen journey: a run made to measure, its worktop with the openings on the CNC",
          "[integration][journey][kitchen]")
{
    const rc::test::TempFolder home("kitchen_journey");

    // --- 1. The kitchen from the catalogue, for a 3 m wall --------------------------------------
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Kitchen);
    editor::setProductName(doc, "Mutfak Demir");
    editor::setDimensions(doc, {3000, 2120, 560});
    const auto base = [&doc] { return doc.product().kitchen()->baseRow().cabinets(); };
    const auto wall = [&doc] { return doc.product().kitchen()->wallRow().cabinets(); };
    // Another drawer cabinet, moved next to the hob; an open shelf above it.
    editor::addCabinet(doc, core::CabinetModule::BaseDrawers, 500.0);
    editor::moveCabinet(doc, base().back()->id(), -1);
    editor::addCabinet(doc, core::CabinetModule::WallOpen, 500.0);
    REQUIRE(base().size() == 6);
    REQUIRE(wall().size() == 6);
    CHECK(base()[4]->interior().drawerStack() != nullptr);
    CHECK(base().back()->box().size().x == 500.0); // the automatic cabinet: 3000 - 2500
    // A deeper sink would hit the front rail: refused. A smaller one fits.
    CHECK_THROWS_AS(editor::setCutout(doc, base()[1]->id(),
                                      core::WorktopCutout{core::CutoutKind::Sink, 560.0, 500.0,
                                                          20.0, 50.0}),
                    editor::ConstructionError);
    editor::setCutout(doc, base()[1]->id(),
                      core::WorktopCutout{core::CutoutKind::Sink, 500.0, 470.0, 20.0, 70.0});
    // The worktop is cut on the nesting centre with the other parts.
    editor::setInCuttingPlan(doc, {doc.product().kitchen()->worktop().id()}, true);

    CHECK(doc.overlaps().empty());
    CHECK(doc.misplacedCutouts().empty());
    CHECK(doc.detailingReport().warnings.empty());

    // --- 2. Saved and opened again: the same kitchen -------------------------------------------
    const auto project = home.path() / "Mutfak Demir.rcproj";
    doc.saveAs(project);
    const editor::Document reopened = editor::Document::open(project);
    CHECK(storage::saveProduct(reopened.product()) == storage::saveProduct(doc.product()));
    const core::Worktop& worktop = reopened.product().kitchen()->worktop();
    REQUIRE(worktop.panel()->cutouts().size() == 2);
    CHECK(worktop.inCuttingPlan());

    // --- 3. The cutting plan on the nesting centre ---------------------------------------------
    // A 3 m worktop does not fit on a 2800 mm sheet: the workshop has worktop slabs on stock,
    // 4100 x 620 as they are sold (the 10 mm margins of the router leave room for 600).
    nesting::StockManager stock;
    stock.add(nesting::Sheet{0, worktop.material(), 4100, 620, 2, false, {}});
    nesting::NestingSettings settings;
    settings.machine = nesting::Machine::NestingCenter;
    settings.optimizer.populationSize = 6;
    settings.optimizer.generations = 3;
    const production::Order order{reopened, 1};
    const nesting::NestingResult plan = production::planCutting(order, stock, settings);
    REQUIRE(plan.unplaced.empty());
    const auto worktopSheet =
        std::find_if(plan.layouts.begin(), plan.layouts.end(), [&](const nesting::SheetLayout& l) {
            return nesting::sameMaterial(l.sheet.material, worktop.material());
        });
    REQUIRE(worktopSheet != plan.layouts.end());
    const auto sheetIndex = static_cast<std::size_t>(worktopSheet - plan.layouts.begin());

    // --- 4. The package -------------------------------------------------------------------------
    const auto folder = production::newPackageFolder(home.path(), production::orderName(reopened));
    const production::Package package = production::writePackage(
        order, &plan, cam::defaultProfile(), {}, exporting::PdfFonts{}, folder);
    CHECK(rc::test::isCompletePdf(rc::test::readFile(package.documents)));

    // The worktop's drawing: its two openings on the CUTOUT layer.
    const auto drawing = std::find_if(
        package.partDrawings.begin(), package.partDrawings.end(),
        [](const std::filesystem::path& p) { return p.filename().string().find("Tezgah") != std::string::npos; });
    REQUIRE(drawing != package.partDrawings.end());
    int openings = 0;
    for (const rc::test::DxfObject& e :
         rc::test::dxfObjects(rc::test::parseDxf(rc::test::readFile(*drawing)), "ENTITIES")) {
        openings += e.type == "POLYLINE" && e.get(8) == "CUTOUT" ? 1 : 0;
    }
    CHECK(openings == 2);

    // The worktop's program: both openings, cut before the worktop comes loose, the whole
    // program inside the sheet and never deeper than the spoilboard cut.
    const std::string program = rc::test::readFile(package.programs.at(sheetIndex));
    const std::size_t sink = program.find("Tezgah 500x470");
    const std::size_t hob = program.find("Tezgah 560x490");
    REQUIRE(sink != std::string::npos);
    REQUIRE(hob != std::string::npos);
    std::size_t outline = std::string::npos;
    for (const detailing::CutListRow& row : reopened.cutList()) {
        if (row.kind == core::ComponentKind::Worktop) {
            outline = program.find("Tezgah " + i18n::mm(row.cut.length) + "x" + i18n::mm(row.cut.width));
        }
    }
    REQUIRE(outline != std::string::npos);
    CHECK(sink < outline);
    CHECK(hob < outline);

    const rc::test::Replay replay = rc::test::replay(program);
    CHECK(replay.lines.back() == "M30");
    for (const rc::test::Move& m : replay.moves) {
        if (rc::test::known(m.to)) {
            CHECK(m.to.x >= -6.0 - 1e-6);
            CHECK(m.to.x <= worktopSheet->sheet.length + 6.0 + 1e-6);
            CHECK(m.to.z >= -cam::defaultProfile().spoilboardCut - 1e-6);
        }
    }
    // The legs, plinth clips and hangers are in the summary's documents: the hardware list.
    CHECK(std::filesystem::exists(package.summary));
}
