#pragma once

#include "readycad/export/Paper.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace rc::exporting {

/// TrueType fonts for PDF documents. Both empty: the built-in Helvetica of PDF viewers (not
/// embedded; Turkish letters through the Windows-1254 code page).
struct PdfFonts {
    std::filesystem::path regular;
    std::filesystem::path bold; ///< empty: the regular font is used for bold text too

    [[nodiscard]] bool builtIn() const noexcept { return regular.empty(); }
};

/// A font with Turkish letters: the bundled Noto Sans in assets/fonts (stage 10) if present,
/// else a system font (Arial, Segoe UI, DejaVu Sans). Built-in fonts if nothing is found.
[[nodiscard]] PdfFonts findPdfFonts();

/// Measures texts with exactly the fonts the PDF will use, so laid-out tables fit.
/// Throws ExportError if a font file cannot be read.
class PdfFontMetrics final : public TextMetrics {
public:
    explicit PdfFontMetrics(const PdfFonts& fonts);
    ~PdfFontMetrics() override;
    PdfFontMetrics(const PdfFontMetrics&) = delete;
    PdfFontMetrics& operator=(const PdfFontMetrics&) = delete;

    [[nodiscard]] Real width(std::string_view text, Real height, bool bold) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// The PDF file as bytes. Pages are drawn from their Drawings: layers give colours, pen widths
/// and dash patterns; texts are placed with the real font metrics. Throws ExportError.
[[nodiscard]] std::string toPdf(const PageSet& pages, const PdfFonts& fonts);

/// Throws ExportError (also when the file cannot be written).
void savePdf(const PageSet& pages, const std::filesystem::path& path, const PdfFonts& fonts);

} // namespace rc::exporting
