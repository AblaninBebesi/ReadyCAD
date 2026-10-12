#pragma once

#include "readycad/core/model/Product.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/export/Paper.hpp"
#include "readycad/export/Pdf.hpp"
#include "readycad/nesting/Nesting.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace rc::exporting {

struct DocumentationOptions {
    PageFormat format;
    bool summary = true;      ///< product data, material need, warnings, front view
    bool cutList = true;      ///< деталировка
    bool hardware = true;     ///< edge banding consumption and hardware list
    bool assembly = true;     ///< front, open front and section A-A with dimensions
    bool partDrawings = true; ///< one sheet per distinct part with its hole table
    bool cuttingPlan = true;  ///< sheet layouts, if DocumentationInput::cutting is given
    std::string productName;  ///< empty: the product's own name or "Adsız"
    std::string company;
    std::string designer;
    std::string date; ///< printed as given; empty: today (dd.mm.yyyy)
};

/// Everything the documents are made of. The editor's Document keeps all of it up to date.
struct DocumentationInput {
    const core::Product& product;
    const std::vector<detailing::CutListRow>& cutList;
    const std::vector<detailing::EdgeBandUsage>& edgeUsage;
    const detailing::DetailingReport& detailing;
    /// Cutting plan of these parts (stage 7); none: the documents have no cutting section.
    const nesting::NestingResult* cutting = nullptr;
};

/// Lays the documents out on pages of the chosen size and orientation. Every page has a frame
/// and a title block (product, sheet title, page n / N, scale, date, company). Long tables
/// continue on the next page; drawings get the largest standard scale that fits.
/// `metrics` must measure with the fonts of the final output (PdfFontMetrics for PDF).
[[nodiscard]] PageSet buildDocumentation(const DocumentationInput& input,
                                         const DocumentationOptions& options,
                                         const TextMetrics& metrics);

/// Today's date as the title blocks print it: "12.10.2026".
[[nodiscard]] std::string todayText();

/// buildDocumentation() + savePdf() with the same fonts. Throws ExportError.
void saveDocumentationPdf(const DocumentationInput& input, const DocumentationOptions& options,
                          const std::filesystem::path& path, const PdfFonts& fonts);

/// Standard reduction scales (ISO 5455 and the usual furniture ones): 1, 2, 5, 10, 15, 20,
/// 25, 50, 100, 200 — returns the smallest denominator (largest scale) for which
/// `fits(1 / denominator)` is true, or the last one.
[[nodiscard]] int chooseScale(const std::function<bool(Real scale)>& fits);

/// "1:20"
[[nodiscard]] std::string scaleText(int denominator);

} // namespace rc::exporting
