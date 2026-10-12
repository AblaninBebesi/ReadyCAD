// Stage 9: texts about the results for people — times, work left for other machines, the
// summary of a package with aligned columns also for Turkish letters.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/production/Summary.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace rc;
using namespace rc::production;
using Catch::Matchers::ContainsSubstring;

namespace {

std::string txt(i18n::TextId id)
{
    return std::string(i18n::text(id));
}

cam::SheetProgram program(double seconds, int edgeHoles, int bottomHoles)
{
    cam::SheetProgram p;
    p.stats.seconds = seconds;
    cam::Leftover l;
    l.edgeHoles = edgeHoles;
    l.bottomHoles = bottomHoles;
    p.leftovers.push_back(l);
    return p;
}

} // namespace

TEST_CASE("Machining times read as minutes and seconds", "[production][summary]")
{
    CHECK(durationText(252.0) == "4 dk 12 sn");
    CHECK(durationText(5.4) == "0 dk 05 sn");
    CHECK(durationText(-1.0) == "0 dk 00 sn");
}

TEST_CASE("Work left for other machines is summed over all sheets", "[production][summary]")
{
    CHECK(leftoverText({}).empty());
    const std::string text = leftoverText({program(10, 3, 0), program(20, 4, 2)});
    CHECK(text == "7 " + txt(i18n::TextId::LeftoverEdge) + "\n2 "
                      + txt(i18n::TextId::LeftoverBottom));
}

TEST_CASE("A stack of identical sheets runs its program again", "[production][summary]")
{
    nesting::NestingResult plan;
    plan.layouts.resize(2);
    plan.layouts[0].count = 3;
    plan.layouts[1].count = 1;
    CHECK(totalMachiningSeconds(plan, {program(100, 0, 0), program(40, 0, 0)}) == 340.0);
}

TEST_CASE("The summary aligns its columns by letters, not by bytes", "[production][summary]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    editor::setProductName(doc, "Gardırop");
    Package package;
    package.folder = "paket";
    package.date = "12.10.2026";
    package.documents = "paket/Gardirop.pdf";
    package.partDrawings = {"paket/DXF/01.dxf", "paket/DXF/02.dxf"};
    const std::string text = summaryText({doc, 2}, nullptr, package);

    // "Sipariş" has 7 letters but 8 bytes, "Parçalar" 8 letters and 9 bytes: padded by letters,
    // the colons stand one under the other.
    CHECK_THAT(text, ContainsSubstring("Sipariş   :  Gardırop × 2\n"));
    CHECK_THAT(text, ContainsSubstring("Tarih     :  12.10.2026\n"));
    CHECK_THAT(text, ContainsSubstring("Parçalar  :  "));
    CHECK_THAT(text, ContainsSubstring(txt(i18n::TextId::NoCuttingPlan)));
    CHECK_THAT(text, ContainsSubstring("Gardirop.pdf\n"));
    // Folders with their number of files.
    CHECK_THAT(text, ContainsSubstring("\nDXF/  2 " + txt(i18n::TextId::FilesWord) + "\n"));
}

TEST_CASE("Parts in each other's space head the summary", "[production][summary]")
{
    // The editor refuses such a change; a project of an older version may still bring it.
    auto product = core::createProduct(core::CatalogItem::Bookcase);
    core::Section& column = *product->carcass()->interior().subsections().front();
    // Two shelves fixed with confirmats at the same height.
    column.addShelf(500.0, core::ShelfMount::Confirmat);
    column.addShelf(500.0, core::ShelfMount::Confirmat);
    const editor::Document doc(product);
    REQUIRE_FALSE(doc.overlaps().empty());

    const auto lines = attentionLines(doc);
    REQUIRE(lines.size() >= doc.overlaps().size());
    CHECK_THAT(lines.front(), ContainsSubstring(txt(i18n::TextId::OverlapWarning)));
    Package package;
    package.folder = "paket";
    const std::string text = summaryText({doc, 1}, nullptr, package);
    CHECK_THAT(text, ContainsSubstring(txt(i18n::TextId::SummaryAttention) + "\n"));
    CHECK_THAT(text, ContainsSubstring("! " + lines.front()));
    // Before the cutting plan: the first thing the workshop reads.
    CHECK(text.find(txt(i18n::TextId::SummaryAttention))
          < text.find(txt(i18n::TextId::SummaryCutting)));
}

TEST_CASE("The message after a package names what was written and what needs attention",
          "[production][summary]")
{
    Package package;
    package.documents = "a.pdf";
    package.partDrawings = {"1.dxf", "2.dxf", "3.dxf"};
    package.programs = {"01.nc"};
    package.cnc = {program(60, 5, 0)};
    nesting::NestingResult plan;
    plan.unplaced.push_back({0, 2});
    const editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    const std::string message = packageMessage({doc, 1}, &plan, package);
    CHECK_THAT(message, ContainsSubstring("1 PDF · 3 " + txt(i18n::TextId::PartDrawingsWord)));
    CHECK_THAT(message, ContainsSubstring("1 " + txt(i18n::TextId::ProgramsWord)));
    CHECK_THAT(message, ContainsSubstring(txt(i18n::TextId::Unplaced) + ": 2"));
    CHECK_THAT(message, ContainsSubstring("5 " + txt(i18n::TextId::LeftoverEdge)));
}
