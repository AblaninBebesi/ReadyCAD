// Stage 7, across modules: the product the user edits is cut from the workshop's stock, the
// remnants go back into the stock file and serve the next job; documents show the plan.
// editor -> detailing (cut list, FlatPart) -> nesting -> storage (stock file) -> export.

#include "readycad/editor/Document.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/export/Documentation.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/SheetDrawing.hpp"
#include "readycad/nesting/Nesting.hpp"
#include "readycad/storage/Storage.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace rc;

namespace {

nesting::NestingSettings quickSettings(nesting::Machine machine)
{
    nesting::NestingSettings s;
    s.machine = machine;
    s.optimizer.populationSize = 8;
    s.optimizer.generations = 4;
    return s;
}

int pieces(const std::vector<nesting::NestingPart>& parts)
{
    int n = 0;
    for (const nesting::NestingPart& p : parts) {
        n += p.quantity;
    }
    return n;
}

} // namespace

TEST_CASE("Two wardrobes are cut from the stock, remnants serve the next job",
          "[integration][nesting]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    editor::setDimensions(doc, {900, 2100, 600});
    const core::Material carcass = doc.product().rules().carcassMaterial;

    nesting::StockManager stock;
    const nesting::SheetId full = stock.add(nesting::Sheet{0, carcass, 2800, 2100, 6, false, {}});

    for (const nesting::Machine machine :
         {nesting::Machine::PanelSaw, nesting::Machine::NestingCenter}) {
        INFO("machine " << static_cast<int>(machine));
        const auto parts = nesting::partsFromCutList(doc.product(), doc.cutList(), 2);
        const nesting::NestingResult result =
            nesting::optimize(parts, stock, quickSettings(machine));
        CHECK(result.unplaced.empty());
        CHECK(result.placedCount() == pieces(parts));

        // The carcass comes from the sheets on stock; the other boards are bought.
        const auto usage = result.usage();
        const auto board = std::find_if(usage.begin(), usage.end(), [&](const auto& u) {
            return nesting::sameMaterial(u.material, carcass);
        });
        REQUIRE(board != usage.end());
        CHECK(board->stock >= 1);
        CHECK(board->purchase == 0);

        // Cut: the stock file afterwards has fewer sheets and the new remnants.
        nesting::StockManager after = stock;
        after.consume(result, "Gardırop × 2");
        const nesting::StockManager saved = storage::loadStock(storage::saveStock(after));
        int remnants = 0;
        for (const nesting::Sheet& s : saved.sheets()) {
            if (s.remnant && nesting::sameMaterial(s.material, carcass)) {
                remnants += s.quantity;
                CHECK(s.note == "Gardırop × 2");
            }
        }
        CHECK(remnants == board->newRemnants);
        REQUIRE(saved.find(full) != nullptr);
        CHECK(saved.find(full)->quantity == 6 - board->stock);

        // A small next job: a remnant is used before a full sheet.
        if (remnants > 0) {
            nesting::NestingPart small;
            small.label = "1";
            small.material = carcass;
            small.outline = nesting::rectangle(nesting::Rect{{0, 0}, {290, 190}});
            const nesting::NestingResult next =
                nesting::optimize({small}, saved, quickSettings(machine));
            REQUIRE(next.layouts.size() == 1);
            CHECK(next.layouts.front().source == nesting::SheetSource::Remnant);
        }

        // Documents: the cutting plan pages and one DXF per sheet layout.
        const exporting::PartSource source{doc.product(), doc.cutList()};
        exporting::DocumentationInput input{doc.product(), doc.cutList(), doc.edgeUsage(),
                                            doc.detailingReport()};
        input.cutting = &result;
        exporting::DocumentationOptions options;
        options.summary = options.cutList = options.hardware = false;
        options.assembly = options.partDrawings = false;
        const exporting::PageSet pages =
            exporting::buildDocumentation(input, options, exporting::ApproxTextMetrics{});
        CHECK(pages.pages.size() >= 1 + result.layouts.size());
        for (std::size_t i = 0; i < result.layouts.size(); ++i) {
            const exporting::Drawing sheet = exporting::drawSheetLayout(result, i, &source);
            CHECK(sheet.count<exporting::Polyline>(exporting::layers::kContour)
                  == result.layouts[i].parts.size());
        }
    }
}
