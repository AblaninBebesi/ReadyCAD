#include "readycad/export/Pdf.hpp"

#include "Utf8.hpp"
#include "readycad/export/Dxf.hpp" // ExportError

#include <hpdf.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <numbers>
#include <vector>

namespace rc::exporting {

namespace {

constexpr Real kPtPerMm = 72.0 / 25.4;

HPDF_REAL pt(Real mm) noexcept
{
    return static_cast<HPDF_REAL>(mm * kPtPerMm);
}

// ---------------------------------------------------------------------------------------------
// libharu document with RAII and error collection
// ---------------------------------------------------------------------------------------------

struct HaruErrors {
    HPDF_STATUS error = HPDF_OK;
    HPDF_STATUS detail = 0;
};

void HPDF_STDCALL collectError(HPDF_STATUS error, HPDF_STATUS detail, void* user)
{
    auto* errors = static_cast<HaruErrors*>(user);
    if (errors->error == HPDF_OK) { // keep the first one: it is the cause
        errors->error = error;
        errors->detail = detail;
    }
}

class HaruDocument {
public:
    HaruDocument() : doc_(HPDF_New(collectError, &errors_))
    {
        if (doc_ == nullptr) {
            throw ExportError("PDF: cannot create a document");
        }
    }
    ~HaruDocument() { HPDF_Free(doc_); }
    HaruDocument(const HaruDocument&) = delete;
    HaruDocument& operator=(const HaruDocument&) = delete;

    [[nodiscard]] HPDF_Doc get() const noexcept { return doc_; }

    /// Throws if libharu reported an error since the start.
    void check(const char* step) const
    {
        if (errors_.error != HPDF_OK) {
            char message[160];
            std::snprintf(message, sizeof message, "PDF: %s failed (libharu error 0x%04X, %u)",
                          step, static_cast<unsigned>(errors_.error),
                          static_cast<unsigned>(errors_.detail));
            throw ExportError(message);
        }
    }

private:
    HaruErrors errors_; // declared first: HPDF_New already gets its address
    HPDF_Doc doc_;
};

std::vector<HPDF_BYTE> readBinary(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw ExportError("PDF: cannot read font " + pathToUtf8(path));
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// ---------------------------------------------------------------------------------------------
// Fonts and text encoding
// ---------------------------------------------------------------------------------------------

/// Unicode -> Windows-1254 (Turkish) for the built-in fonts; Cyrillic in Latin letters, '?' for
/// anything else it lacks.
std::string toCp1254(std::string_view utf8)
{
    std::string out;
    for (const char32_t c : decodeUtf8(utf8)) {
        if (c >= 0x400 && c <= 0x4FF) {
            // Helvetica has no Cyrillic: the letters as in a passport rather than "?".
            const char letter[] = {static_cast<char>(0xC0 | (c >> 6)),
                                   static_cast<char>(0x80 | (c & 0x3F))};
            out += asciiFold(std::string_view(letter, 2));
            continue;
        }
        unsigned char byte = '?';
        if (c < 0x80) {
            byte = static_cast<unsigned char>(c);
        } else if (c >= 0xA0 && c <= 0xFF && c != 0xD0 && c != 0xDD && c != 0xDE && c != 0xF0
                   && c != 0xFD && c != 0xFE) {
            byte = static_cast<unsigned char>(c); // same as Latin-1 there
        } else {
            switch (c) {
            case U'Ğ': byte = 0xD0; break;
            case U'İ': byte = 0xDD; break;
            case U'Ş': byte = 0xDE; break;
            case U'ğ': byte = 0xF0; break;
            case U'ı': byte = 0xFD; break;
            case U'ş': byte = 0xFE; break;
            case U'€': byte = 0x80; break;
            case U'…': byte = 0x85; break;
            case U'•': byte = 0x95; break;
            case U'–': byte = 0x96; break;
            case U'—': byte = 0x97; break;
            case U'‘': byte = 0x91; break;
            case U'’': byte = 0x92; break;
            case U'“': byte = 0x93; break;
            case U'”': byte = 0x94; break;
            case U'⌀': byte = 0xD8; break; // closest: Ø
            default:   break;
            }
        }
        out += static_cast<char>(byte);
    }
    return out;
}

/// The fonts of one libharu document.
struct HaruFonts {
    HPDF_Font regular = nullptr;
    HPDF_Font bold = nullptr;
    bool unicode = false;    ///< TrueType + UTF-8; otherwise Helvetica + CP1254
    Real capHeight = 0.716;  ///< of the font size: text height (capitals) -> font size

    [[nodiscard]] HPDF_Font font(bool isBold) const noexcept { return isBold ? bold : regular; }
    [[nodiscard]] std::string encode(std::string_view text) const
    {
        return unicode ? std::string(text) : toCp1254(text);
    }
    [[nodiscard]] Real fontSize(Real capitalHeight) const noexcept
    {
        return capitalHeight / capHeight;
    }
};

HaruFonts loadFonts(HaruDocument& doc, const PdfFonts& files)
{
    HaruFonts fonts;
    if (files.builtIn()) {
        fonts.regular = HPDF_GetFont(doc.get(), "Helvetica", "CP1254");
        fonts.bold = HPDF_GetFont(doc.get(), "Helvetica-Bold", "CP1254");
        fonts.capHeight = 0.718; // Helvetica AFM: CapHeight 718
        doc.check("loading the built-in fonts");
        return fonts;
    }

    HPDF_UseUTFEncodings(doc.get());
    HPDF_SetCurrentEncoder(doc.get(), "UTF-8");
    const auto load = [&](const std::filesystem::path& path) {
        const std::vector<HPDF_BYTE> data = readBinary(path);
        const char* name = HPDF_LoadTTFontFromMemory(doc.get(), data.data(),
                                                     static_cast<HPDF_UINT>(data.size()), HPDF_TRUE);
        doc.check("loading a TrueType font");
        return HPDF_GetFont(doc.get(), name, "UTF-8");
    };
    fonts.unicode = true;
    fonts.regular = load(files.regular);
    fonts.bold = files.bold.empty() ? fonts.regular : load(files.bold);
    doc.check("loading fonts");

    const Real cap = static_cast<Real>(HPDF_Font_GetCapHeight(fonts.regular)) / 1000.0;
    if (cap > 0.4 && cap < 1.0) {
        fonts.capHeight = cap;
    }
    return fonts;
}

class HaruMetrics final : public TextMetrics {
public:
    explicit HaruMetrics(const HaruFonts& fonts) : fonts_(fonts) {}

    [[nodiscard]] Real width(std::string_view text, Real height, bool bold) const override
    {
        if (text.empty()) {
            return 0.0;
        }
        const std::string encoded = fonts_.encode(text);
        const HPDF_TextWidth tw = HPDF_Font_TextWidth(
            fonts_.font(bold), reinterpret_cast<const HPDF_BYTE*>(encoded.data()),
            static_cast<HPDF_UINT>(encoded.size()));
        return static_cast<Real>(tw.width) / 1000.0 * fonts_.fontSize(height);
    }

private:
    const HaruFonts& fonts_;
};

// ---------------------------------------------------------------------------------------------
// Drawing a page
// ---------------------------------------------------------------------------------------------

Real penWidth(LineWeight weight) noexcept
{
    switch (weight) {
    case LineWeight::Thin:   return 0.18;
    case LineWeight::Normal: return 0.25;
    case LineWeight::Thick:  return 0.5;
    }
    return 0.25;
}

class PageRenderer {
public:
    PageRenderer(HPDF_Page page, const HaruFonts& fonts, const TextMetrics& metrics)
        : page_(page), fonts_(fonts), metrics_(metrics)
    {
        HPDF_Page_SetLineCap(page_, HPDF_ROUND_END);
        HPDF_Page_SetLineJoin(page_, HPDF_ROUND_JOIN);
    }

    void draw(const Drawing& drawing)
    {
        for (const Entity& e : drawing.entities()) {
            const Layer& layer = drawing.layers()[e.layer];
            std::visit([&](const auto& shape) { draw(layer, shape); }, e.shape);
        }
    }

private:
    void useStroke(const Layer& layer)
    {
        const Real width = penWidth(layer.weight);
        if (width != width_) {
            HPDF_Page_SetLineWidth(page_, pt(width));
            width_ = width;
        }
        if (layer.lineType != lineType_) {
            // Short dashes: hole circles of a few millimetres must still look dashed.
            static constexpr std::array<Real, 2> kDashed{1.2, 0.8};
            static constexpr std::array<Real, 4> kCenter{6.0, 1.0, 1.0, 1.0};
            std::array<HPDF_REAL, 4> pattern{};
            HPDF_UINT count = 0;
            if (layer.lineType == LineType::Dashed) {
                for (const Real v : kDashed) {
                    pattern[count++] = pt(v);
                }
            } else if (layer.lineType == LineType::Center) {
                for (const Real v : kCenter) {
                    pattern[count++] = pt(v);
                }
            }
            HPDF_Page_SetDash(page_, count > 0 ? pattern.data() : nullptr, count, 0);
            lineType_ = layer.lineType;
        }
        useColor(layer.print, true);
    }

    void useColor(Rgb color, bool stroke)
    {
        const auto c = [](std::uint8_t v) { return static_cast<HPDF_REAL>(v) / 255.0F; };
        if (stroke) {
            HPDF_Page_SetRGBStroke(page_, c(color.r), c(color.g), c(color.b));
        } else {
            HPDF_Page_SetRGBFill(page_, c(color.r), c(color.g), c(color.b));
        }
    }

    void draw(const Layer& layer, const Line& line)
    {
        useStroke(layer);
        HPDF_Page_MoveTo(page_, pt(line.a.x), pt(line.a.y));
        HPDF_Page_LineTo(page_, pt(line.b.x), pt(line.b.y));
        HPDF_Page_Stroke(page_);
    }

    void draw(const Layer& layer, const Polyline& poly)
    {
        if (poly.points.size() < 2) {
            return;
        }
        if (poly.filled) {
            useColor(layer.print, false);
        } else {
            useStroke(layer);
        }
        HPDF_Page_MoveTo(page_, pt(poly.points[0].x), pt(poly.points[0].y));
        for (std::size_t i = 1; i < poly.points.size(); ++i) {
            HPDF_Page_LineTo(page_, pt(poly.points[i].x), pt(poly.points[i].y));
        }
        if (poly.filled) {
            HPDF_Page_ClosePath(page_);
            HPDF_Page_Fill(page_);
        } else if (poly.closed) {
            HPDF_Page_ClosePathStroke(page_);
        } else {
            HPDF_Page_Stroke(page_);
        }
    }

    void draw(const Layer& layer, const Circle& circle)
    {
        useStroke(layer);
        HPDF_Page_Circle(page_, pt(circle.center.x), pt(circle.center.y), pt(circle.radius));
        HPDF_Page_Stroke(page_);
    }

    void draw(const Layer& layer, const Text& text)
    {
        if (text.text.empty()) {
            return;
        }
        const Vec2 start = baselineStart(text, metrics_);
        const Real radians = text.angle * std::numbers::pi / 180.0;
        const auto cosA = static_cast<HPDF_REAL>(std::cos(radians));
        const auto sinA = static_cast<HPDF_REAL>(std::sin(radians));
        const std::string encoded = fonts_.encode(text.text);
        useColor(layer.print, false);
        HPDF_Page_BeginText(page_);
        HPDF_Page_SetFontAndSize(page_, fonts_.font(text.bold), pt(fonts_.fontSize(text.height)));
        HPDF_Page_SetTextMatrix(page_, cosA, sinA, -sinA, cosA, pt(start.x), pt(start.y));
        HPDF_Page_ShowText(page_, encoded.c_str());
        HPDF_Page_EndText(page_);
    }

    void draw(const Layer& layer, const Dimension& dimension)
    {
        for (const Shape& part : explode(dimension, metrics_)) {
            std::visit([&](const auto& shape) { draw(layer, shape); }, part);
        }
    }

    HPDF_Page page_;
    const HaruFonts& fonts_;
    const TextMetrics& metrics_;
    Real width_ = -1.0;
    LineType lineType_ = LineType::Continuous;
};

std::string writeDocument(const PageSet& pages, const PdfFonts& files)
{
    HaruDocument doc;
    HPDF_SetCompressionMode(doc.get(), HPDF_COMP_NONE);
    const HaruFonts fonts = loadFonts(doc, files);
    const HaruMetrics metrics(fonts);

    // Document information in plain ASCII: viewers read it without an encoding.
    HPDF_SetInfoAttr(doc.get(), HPDF_INFO_CREATOR, "ReadyCAD");
    HPDF_SetInfoAttr(doc.get(), HPDF_INFO_PRODUCER, "ReadyCAD (libHaru)");
    if (!pages.title.empty()) {
        HPDF_SetInfoAttr(doc.get(), HPDF_INFO_TITLE, asciiFold(pages.title).c_str());
    }
    if (!pages.author.empty()) {
        HPDF_SetInfoAttr(doc.get(), HPDF_INFO_AUTHOR, asciiFold(pages.author).c_str());
    }
    if (!pages.subject.empty()) {
        HPDF_SetInfoAttr(doc.get(), HPDF_INFO_SUBJECT, asciiFold(pages.subject).c_str());
    }
    // Bookmarks go through the Turkish single-byte table: libharu turns it into UTF-16.
    HPDF_Encoder outlineEncoder = HPDF_GetEncoder(doc.get(), "CP1254");
    doc.check("preparing the document");

    for (const Page& page : pages.pages) {
        HPDF_Page p = HPDF_AddPage(doc.get());
        HPDF_Page_SetWidth(p, pt(page.size.x));
        HPDF_Page_SetHeight(p, pt(page.size.y));
        PageRenderer(p, fonts, metrics).draw(page.drawing);
        if (!page.bookmark.empty()) {
            HPDF_Outline outline = HPDF_CreateOutline(doc.get(), nullptr,
                                                      toCp1254(page.bookmark).c_str(),
                                                      outlineEncoder);
            HPDF_Outline_SetDestination(outline, HPDF_Page_CreateDestination(p));
        }
        doc.check("drawing a page");
    }
    if (!pages.pages.empty()) {
        HPDF_SetPageMode(doc.get(), HPDF_PAGE_MODE_USE_OUTLINE);
    }

    HPDF_SaveToStream(doc.get());
    doc.check("writing the document");
    std::string bytes;
    bytes.resize(HPDF_GetStreamSize(doc.get()));
    HPDF_UINT32 size = static_cast<HPDF_UINT32>(bytes.size());
    HPDF_ReadFromStream(doc.get(), reinterpret_cast<HPDF_BYTE*>(bytes.data()), &size);
    bytes.resize(size);
    return bytes;
}

} // namespace

// ---------------------------------------------------------------------------------------------

PdfFonts findPdfFonts()
{
    std::filesystem::path windows = "C:/Windows";
    if (const char* dir = std::getenv("WINDIR")) {
        windows = dir;
    }
    const std::filesystem::path winFonts = windows / "Fonts";
    const std::array<PdfFonts, 5> candidates = {{
        {"assets/fonts/NotoSans-Regular.ttf", "assets/fonts/NotoSans-Bold.ttf"},
        {winFonts / "arial.ttf", winFonts / "arialbd.ttf"},
        {winFonts / "segoeui.ttf", winFonts / "segoeuib.ttf"},
        {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
        {"/System/Library/Fonts/Supplemental/Arial.ttf",
         "/System/Library/Fonts/Supplemental/Arial Bold.ttf"},
    }};
    for (const PdfFonts& fonts : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(fonts.regular, ec)) {
            PdfFonts found = fonts;
            if (!std::filesystem::exists(found.bold, ec)) {
                found.bold.clear();
            }
            return found;
        }
    }
    return {};
}

struct PdfFontMetrics::Impl {
    HaruDocument doc;
    HaruFonts fonts;
    std::unique_ptr<HaruMetrics> metrics;
};

PdfFontMetrics::PdfFontMetrics(const PdfFonts& fonts) : impl_(std::make_unique<Impl>())
{
    impl_->fonts = loadFonts(impl_->doc, fonts);
    impl_->metrics = std::make_unique<HaruMetrics>(impl_->fonts);
}

PdfFontMetrics::~PdfFontMetrics() = default;

Real PdfFontMetrics::width(std::string_view text, Real height, bool bold) const
{
    return impl_->metrics->width(text, height, bold);
}

std::string toPdf(const PageSet& pages, const PdfFonts& fonts)
{
    return writeDocument(pages, fonts);
}

void savePdf(const PageSet& pages, const std::filesystem::path& path, const PdfFonts& fonts)
{
    const std::string bytes = writeDocument(pages, fonts);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw ExportError("cannot write " + pathToUtf8(path));
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.flush();
    if (!file) {
        throw ExportError("cannot write " + pathToUtf8(path));
    }
}

} // namespace rc::exporting
