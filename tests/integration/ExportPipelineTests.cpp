// Stage 6, across modules: what the user edits is what the documents show.
// editor (Document + operations) -> detailing (derived data) -> export (DXF, PDF pages).

#include "readycad/editor/Document.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/export/Documentation.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/Pdf.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <variant>

using namespace rc;

namespace {

bool pageShows(const exporting::PageSet& pages, const std::string& text)
{
    for (const exporting::Page& page : pages.pages) {
        for (const exporting::Entity& e : page.drawing.entities()) {
            if (const auto* t = std::get_if<exporting::Text>(&e.shape); t && t->text == text) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

TEST_CASE("An edit in the editor reaches the PDF and the DXF", "[integration][export]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    editor::setDimensions(doc, {900, 2100, 600});

    const exporting::DocumentationInput input{doc.product(), doc.cutList(), doc.edgeUsage(),
                                              doc.detailingReport()};
    exporting::DocumentationOptions options;
    options.date = "07.10.2026";
    const exporting::PdfFontMetrics metrics(exporting::PdfFonts{});
    const exporting::PageSet pages = exporting::buildDocumentation(input, options, metrics);

    // The new overall size is in the title blocks, the new top panel in the cut list.
    CHECK(pageShows(pages, "900 × 2100 × 618"));
    CHECK(pageShows(pages, "900 × 600"));
    const std::string pdf = exporting::toPdf(pages, exporting::PdfFonts{});
    CHECK(pdf.starts_with("%PDF-"));

    // All parts in one DXF: one contour per distinct part, each at its finish size.
    const exporting::Drawing sheet = exporting::drawAllParts(doc.product(), doc.cutList());
    CHECK(sheet.count<exporting::Polyline>(exporting::layers::kContour) == doc.cutList().size());
    const std::string dxf = exporting::toDxf(sheet);
    CHECK(dxf.find("AC1009") != std::string::npos);
    CHECK(dxf.ends_with("EOF\n"));
}
