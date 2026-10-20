#include "readycad/export/Documentation.hpp"

#include "readycad/core/model/Kitchen.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/detailing/HardwareList.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/ProductViews.hpp"
#include "readycad/export/SheetDrawing.hpp"
#include "readycad/i18n/Labels.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <deque>
#include <map>

namespace rc::exporting {

using i18n::TextId;

namespace {

// Sizes on paper, mm. Text sizes are heights of capital letters.
constexpr Real kHeading = 4.0;
constexpr Real kBody = 2.5;
constexpr Real kTable = 2.2;
constexpr Real kSmall = 1.6;
constexpr Real kRow = 5.6;          // table row height
constexpr Real kPad = 1.2;          // cell padding
constexpr Real kTitleBlockWidth = 180.0;
constexpr Real kTitleBlockRow = 7.0;
constexpr Real kGap = 6.0;          // between blocks on a page

std::string txt(TextId id)
{
    return std::string(i18n::text(id));
}

std::string mm(Real value)
{
    return i18n::mm(value);
}

std::string sizeText(Real a, Real b)
{
    return mm(a) + " × " + mm(b);
}

std::string sizeText(Real a, Real b, Real c)
{
    return mm(a) + " × " + mm(b) + " × " + mm(c);
}

} // namespace

std::string todayText()
{
    const auto days = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    const std::chrono::year_month_day ymd(days);
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%02u.%02u.%04d", static_cast<unsigned>(ymd.day()),
                  static_cast<unsigned>(ymd.month()), static_cast<int>(ymd.year()));
    return buffer;
}

namespace {

/// A kitchen with runs on side walls: drawn as a plan and wall by wall.
bool cornerKitchen(const core::Product& product)
{
    const core::Kitchen* kitchen = product.kitchen();
    return kitchen != nullptr && kitchen->runs().size() > 1;
}

/// Removes the last UTF-8 code point.
void popCodePoint(std::string& text)
{
    while (!text.empty()) {
        const auto byte = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((byte & 0xC0U) != 0x80U) {
            break;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// A page under construction
// ---------------------------------------------------------------------------------------------

struct PageLayers {
    std::size_t frame;
    std::size_t grid;
    std::size_t fill;
    std::size_t text;
    std::size_t muted;
};

PageLayers addPageLayers(Drawing& d)
{
    return {
        d.addLayer({"FRAME", 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thick}),
        d.addLayer({"GRID", 8, {120, 120, 120}, LineType::Continuous, LineWeight::Thin}),
        d.addLayer({"FILL", 9, {232, 232, 232}, LineType::Continuous, LineWeight::Thin}),
        d.addLayer({"PAGE_TEXT", 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thin}),
        d.addLayer({"PAGE_TEXT_MUTED", 8, {110, 110, 110}, LineType::Continuous,
                    LineWeight::Thin}),
    };
}

/// Drawing helpers for one page.
class Canvas {
public:
    Canvas(Drawing& drawing, const TextMetrics& metrics)
        : drawing_(drawing), metrics_(metrics), layers_(addPageLayers(drawing))
    {
    }

    [[nodiscard]] Drawing& drawing() noexcept { return drawing_; }
    [[nodiscard]] const TextMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] const PageLayers& layers() const noexcept { return layers_; }

    void text(Vec2 at, const std::string& value, Real height, bool bold = false,
              HAlign h = HAlign::Left, VAlign v = VAlign::Baseline, bool muted = false)
    {
        drawing_.text(muted ? layers_.muted : layers_.text, {at, value, height, 0.0, h, v, bold});
    }

    /// The text shortened with "…" so that it is at most `width` wide.
    [[nodiscard]] std::string fit(std::string value, Real width, Real height, bool bold) const
    {
        if (metrics_.width(value, height, bold) <= width) {
            return value;
        }
        while (!value.empty() && metrics_.width(value + "…", height, bold) > width) {
            popCodePoint(value);
        }
        return value + "…";
    }

    void frame(Vec2 min, Vec2 max) { drawing_.rectangle(layers_.frame, min, max); }
    void grid(Vec2 a, Vec2 b) { drawing_.line(layers_.grid, a, b); }
    void fill(Vec2 min, Vec2 max) { drawing_.rectangle(layers_.fill, min, max, true); }

private:
    Drawing& drawing_;
    const TextMetrics& metrics_;
    PageLayers layers_;
};

// ---------------------------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------------------------

struct Column {
    std::string header;
    Real weight = 1.0;
    HAlign align = HAlign::Left;
};

struct Row {
    std::vector<std::string> cells;
    bool bold = false;
};

struct Table {
    std::vector<Column> columns;
    std::vector<Row> rows;
    Real textHeight = kTable;
    bool header = true;
};

/// Draws the header and rows from `first` downwards while they fit into `height`; at least one
/// row. Returns the index of the first row not drawn and the height used.
std::pair<std::size_t, Real> drawTable(Canvas& canvas, Vec2 topLeft, Real width, Real height,
                                       const Table& table, std::size_t first)
{
    Real total = 0.0;
    for (const Column& c : table.columns) {
        total += c.weight;
    }
    std::vector<Real> xs{topLeft.x};
    for (const Column& c : table.columns) {
        xs.push_back(xs.back() + width * c.weight / total);
    }
    const Real th = table.textHeight;
    const Real rowHeight = kRow * th / kTable;

    const auto drawRow = [&](Real top, const std::vector<std::string>& cells, bool bold) {
        const Real baseline = top - rowHeight / 2.0 - th / 2.0;
        for (std::size_t i = 0; i < table.columns.size() && i < cells.size(); ++i) {
            const Real cellWidth = xs[i + 1] - xs[i] - 2.0 * kPad;
            const std::string value = canvas.fit(cells[i], cellWidth, th, bold);
            Real x = xs[i] + kPad;
            if (table.columns[i].align == HAlign::Center) {
                x = (xs[i] + xs[i + 1]) / 2.0;
            } else if (table.columns[i].align == HAlign::Right) {
                x = xs[i + 1] - kPad;
            }
            canvas.text(Vec2(x, baseline), value, th, bold, table.columns[i].align);
        }
        canvas.grid(Vec2(topLeft.x, top - rowHeight), Vec2(xs.back(), top - rowHeight));
    };

    Real y = topLeft.y;
    canvas.grid(Vec2(topLeft.x, y), Vec2(xs.back(), y));
    if (table.header) {
        canvas.fill(Vec2(topLeft.x, y - rowHeight), Vec2(xs.back(), y));
        std::vector<std::string> headers;
        for (const Column& c : table.columns) {
            headers.push_back(c.header);
        }
        drawRow(y, headers, true);
        y -= rowHeight;
    }
    std::size_t next = first;
    while (next < table.rows.size()
           && (next == first || topLeft.y - (y - rowHeight) <= height + 1e-9)) {
        drawRow(y, table.rows[next].cells, table.rows[next].bold);
        y -= rowHeight;
        ++next;
    }
    for (const Real x : xs) {
        canvas.grid(Vec2(x, topLeft.y), Vec2(x, y));
    }
    return {next, topLeft.y - y};
}

// ---------------------------------------------------------------------------------------------
// Pages with frame and title block
// ---------------------------------------------------------------------------------------------

class Composer {
public:
    Composer(const DocumentationOptions& options, std::string product, std::string productSize,
             const TextMetrics& metrics)
        : options_(options)
        , product_(std::move(product))
        , productSize_(std::move(productSize))
        , metrics_(metrics)
        , size_(pageSize(options.format))
    {
        const Margins m;
        frameMin_ = Vec2(m.left, m.bottom);
        frameMax_ = Vec2(size_.x - m.right, size_.y - m.top);
        // Content: inside the frame, above the title block, below the page heading.
        contentMin_ = Vec2(frameMin_.x + 4.0, frameMin_.y + 3.0 * kTitleBlockRow + 5.0);
        contentMax_ = Vec2(frameMax_.x - 4.0, frameMax_.y - 4.0 - kHeading - 5.0);
    }

    [[nodiscard]] Vec2 contentMin() const noexcept { return contentMin_; }
    [[nodiscard]] Vec2 contentMax() const noexcept { return contentMax_; }
    [[nodiscard]] Vec2 contentSize() const noexcept { return contentMax_ - contentMin_; }

    Canvas newPage(const std::string& title, const std::string& bookmark)
    {
        pages_.push_back({size_, Drawing{}, bookmark});
        titles_.push_back(title);
        scales_.emplace_back("—");
        Canvas canvas(pages_.back().drawing, metrics_);
        canvas.frame(frameMin_, frameMax_);
        canvas.text(Vec2(frameMin_.x + 4.0, frameMax_.y - 4.0), title, kHeading, true,
                    HAlign::Left, VAlign::Top);
        return canvas;
    }

    void setScale(const std::string& scale) { scales_.back() = scale; }

    PageSet finish(std::string title)
    {
        PageSet set;
        set.title = std::move(title);
        set.author = options_.company.empty() ? options_.designer : options_.company;
        set.subject = txt(TextId::ReportTitle);
        const std::size_t count = pages_.size();
        for (std::size_t i = 0; i < count; ++i) {
            Canvas canvas(pages_[i].drawing, metrics_);
            titleBlock(canvas, titles_[i], scales_[i], i + 1, count);
            set.pages.push_back(std::move(pages_[i]));
        }
        return set;
    }

private:
    /// ISO 7200-like title block in the lower right corner, 180 x 21 mm.
    void titleBlock(Canvas& c, const std::string& title, const std::string& scale,
                    std::size_t page, std::size_t count) const
    {
        const Real w = std::min(kTitleBlockWidth, frameMax_.x - frameMin_.x);
        const Vec2 min(frameMax_.x - w, frameMin_.y);
        const Real h = kTitleBlockRow;
        const std::array<Real, 4> xs{min.x, min.x + w * 0.28, min.x + w * 0.72, min.x + w};
        c.frame(min, Vec2(min.x + w, min.y + 3.0 * h));
        for (int row = 1; row < 3; ++row) {
            c.grid(Vec2(min.x, min.y + row * h), Vec2(min.x + w, min.y + row * h));
        }
        for (std::size_t i = 1; i < 3; ++i) {
            c.grid(Vec2(xs[i], min.y), Vec2(xs[i], min.y + 3.0 * h));
        }
        // row 0 is the bottom one
        const auto cell = [&](std::size_t col, int row, const std::string& label,
                              const std::string& value, bool bold = false) {
            const Real left = xs[col] + 1.2;
            const Real top = min.y + (row + 1) * h;
            const Real width = xs[col + 1] - xs[col] - 2.4;
            if (!label.empty()) {
                c.text(Vec2(left, top - 0.8), c.fit(label, width, kSmall, false), kSmall, false,
                       HAlign::Left, VAlign::Top, true);
            }
            c.text(Vec2(left, top - h + 1.2), c.fit(value, width, kBody, bold), kBody, bold);
        };
        cell(0, 2, "", "ReadyCAD", true);
        cell(0, 1, txt(TextId::TbCompany), options_.company);
        cell(0, 0, txt(TextId::TbDesigner), options_.designer);
        cell(1, 2, txt(TextId::TbProduct), product_, true);
        cell(1, 1, txt(TextId::TbTitle), title);
        cell(1, 0, txt(TextId::OverallSize), productSize_);
        cell(2, 2, txt(TextId::TbSheet), std::to_string(page) + " / " + std::to_string(count));
        cell(2, 1, txt(TextId::TbScale), scale);
        cell(2, 0, txt(TextId::TbDate), options_.date.empty() ? todayText() : options_.date);
    }

    const DocumentationOptions& options_;
    std::string product_;
    std::string productSize_;
    const TextMetrics& metrics_;
    Vec2 size_;
    Vec2 frameMin_{0.0};
    Vec2 frameMax_{0.0};
    Vec2 contentMin_{0.0};
    Vec2 contentMax_{0.0};
    std::deque<Page> pages_; // deque: canvases keep pointing at their page while pages are added
    std::vector<std::string> titles_;
    std::vector<std::string> scales_;
};

/// A table over as many pages as it needs, starting on a new page.
/// The table on as many pages as it needs; `footnote` (if any) under its last row.
void paginate(Composer& composer, const std::string& title, const std::string& bookmark,
              const Table& table, const std::string& footnote = {})
{
    std::size_t next = 0;
    bool first = true;
    do {
        Canvas canvas = composer.newPage(first ? title : title + " " + txt(TextId::Continued),
                                         first ? bookmark : std::string());
        const auto [rest, used] =
            drawTable(canvas, Vec2(composer.contentMin().x, composer.contentMax().y),
                      composer.contentSize().x, composer.contentSize().y, table, next);
        next = rest;
        first = false;
        if (next == table.rows.size() && !footnote.empty()) {
            const Real y = composer.contentMax().y - used - kGap - kTable;
            if (y >= composer.contentMin().y) {
                canvas.text(Vec2(composer.contentMin().x, y),
                            canvas.fit(footnote, composer.contentSize().x, kTable, false), kTable,
                            false);
            }
        }
    } while (next < table.rows.size());
}

/// Appends a drawing scaled by `scale` so that its bounds are centred in [min, max].
void place(Canvas& canvas, const Drawing& drawing, Real scale, Vec2 min, Vec2 max)
{
    const Bounds b = drawing.bounds(canvas.metrics());
    const Vec2 centre = (min + max) * 0.5;
    canvas.drawing().append(drawing, scale, centre - b.center() * scale);
}

// ---------------------------------------------------------------------------------------------
// Contents
// ---------------------------------------------------------------------------------------------

std::string edgeCell(const core::EdgeBand& band)
{
    return band.present() ? mm(band.thickness) : "—";
}

Table cutListTable(const std::vector<detailing::CutListRow>& cutList)
{
    Table table;
    table.columns = {
        {txt(TextId::ColNo), 3.0, HAlign::Right},
        {txt(TextId::ColPart), 13.0},
        {txt(TextId::ColMaterial), 12.0},
        {txt(TextId::ColQuantity), 4.5, HAlign::Right},
        {txt(TextId::ColFinish), 10.0, HAlign::Right},
        {txt(TextId::ColCut), 10.0, HAlign::Right},
        {txt(TextId::EdgeL1), 3.5, HAlign::Center},
        {txt(TextId::EdgeL2), 3.5, HAlign::Center},
        {txt(TextId::EdgeW1), 3.5, HAlign::Center},
        {txt(TextId::EdgeW2), 3.5, HAlign::Center},
        {txt(TextId::ColGrain), 7.0, HAlign::Center},
    };
    int total = 0;
    for (std::size_t i = 0; i < cutList.size(); ++i) {
        const detailing::CutListRow& r = cutList[i];
        total += r.quantity;
        table.rows.push_back({{std::to_string(i + 1), partName(r) + (r.inCuttingPlan ? "" : " *"),
                               r.material.name,
                               std::to_string(r.quantity), sizeText(r.finishLength, r.finishWidth),
                               sizeText(r.cut.length, r.cut.width), edgeCell(r.edges[0]),
                               edgeCell(r.edges[1]), edgeCell(r.edges[2]), edgeCell(r.edges[3]),
                               r.keepGrainDirection ? txt(TextId::GrainKept) : "—"}});
    }
    table.rows.push_back({{"", txt(TextId::TotalParts), "", std::to_string(total)}, true});
    return table;
}

std::string twoDecimals(Real value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.2f", value);
    return buffer;
}

// --- Summary ----------------------------------------------------------------------------------

void summaryPage(Composer& composer, const DocumentationInput& in, const std::string& product)
{
    Canvas canvas = composer.newPage(txt(TextId::PageSummary), txt(TextId::PageSummary));
    const Vec2 min = composer.contentMin();
    const Vec2 max = composer.contentMax();
    const Real columnWidth = std::min(120.0, (max.x - min.x) * 0.5);

    int parts = 0;
    for (const detailing::CutListRow& r : in.cutList) {
        parts += r.quantity;
    }
    Real edgeMeters = 0.0;
    for (const detailing::EdgeBandUsage& u : in.edgeUsage) {
        edgeMeters += u.totalLength / 1000.0;
    }
    const core::Aabb bounds = core::panelBounds(in.product);
    const core::Size3D& dims = in.product.dimensions();

    Table facts;
    facts.header = false;
    facts.columns = {{"", 1.3}, {"", 1.0}};
    facts.rows = {
        {{txt(TextId::TbProduct), product}, true},
        {{txt(TextId::OverallSize), sizeText(bounds.size().x, bounds.size().y, bounds.size().z)}},
        {{txt(TextId::CarcassSize), sizeText(dims.width, dims.height, dims.depth)}},
        {{txt(TextId::TotalParts), std::to_string(parts)}},
        {{txt(TextId::DistinctParts), std::to_string(in.cutList.size())}},
        {{txt(TextId::EdgeUsage), twoDecimals(edgeMeters) + " m"}},
        {{txt(TextId::JointCount), std::to_string(in.detailing.joints)}},
        {{txt(TextId::HoleCount), std::to_string(in.detailing.drillings)}},
        {{txt(TextId::GrooveCount), std::to_string(in.detailing.grooves)}},
    };
    Real y = max.y;
    y -= drawTable(canvas, Vec2(min.x, y), columnWidth, y - min.y, facts, 0).second + kGap;

    // Material need, per board.
    std::vector<std::string> order;
    std::map<std::string, std::pair<int, Real>> need;
    for (const detailing::CutListRow& r : in.cutList) {
        if (!need.contains(r.material.name)) {
            order.push_back(r.material.name);
        }
        auto& [count, area] = need[r.material.name];
        count += r.quantity;
        area += r.cut.length * r.cut.width * r.quantity / 1e6;
    }
    canvas.text(Vec2(min.x, y), txt(TextId::MaterialNeed), kBody, true, HAlign::Left, VAlign::Top);
    y -= kBody + 2.5;
    Table materials;
    materials.columns = {{txt(TextId::ColMaterial), 2.0},
                         {txt(TextId::ColQuantity), 0.8, HAlign::Right},
                         {txt(TextId::ColArea), 1.0, HAlign::Right}};
    for (const std::string& name : order) {
        materials.rows.push_back(
            {{name, std::to_string(need[name].first), twoDecimals(need[name].second)}});
    }
    y -= drawTable(canvas, Vec2(min.x, y), columnWidth, y - min.y, materials, 0).second + kGap;

    // Detailing warnings, as many as fit.
    canvas.text(Vec2(min.x, y), txt(TextId::Warnings), kBody, true, HAlign::Left, VAlign::Top);
    y -= kBody + 2.5;
    std::vector<std::string> warnings;
    if (const core::Kitchen* kitchen = in.product.kitchen()) {
        for (const core::Worktop* worktop : kitchen->longWorktops(in.product.rules())) {
            warnings.push_back(txt(TextId::LongWorktopWarning) + ": " + i18n::displayName(*worktop)
                               + " " + mm(worktop->box().size().x) + " mm");
        }
    }
    warnings.insert(warnings.end(), in.detailing.warnings.begin(), in.detailing.warnings.end());
    if (warnings.empty()) {
        canvas.text(Vec2(min.x, y), canvas.fit(txt(TextId::NoWarnings), columnWidth, kTable, false),
                    kTable, false, HAlign::Left, VAlign::Top);
    }
    for (const std::string& warning : warnings) {
        if (y < min.y + 2.0 * kTable) {
            break;
        }
        canvas.text(Vec2(min.x, y), canvas.fit("• " + warning, columnWidth, kTable, false), kTable,
                    false, HAlign::Left, VAlign::Top);
        y -= kTable * 1.9;
    }

    // A picture of the product on the right: from the front, a corner kitchen from above.
    const Vec2 picMin(min.x + columnWidth + 2.0 * kGap, min.y + 8.0);
    const Vec2 picMax = max;
    const ProductView picture = cornerKitchen(in.product) ? ProductView::Plan : ProductView::Front;
    const Bounds extent = viewExtent(in.product, picture);
    if (!extent.empty && picMax.x - picMin.x > 30.0) {
        const int denominator = chooseScale([&](Real s) {
            return extent.size().x * s <= picMax.x - picMin.x
                && extent.size().y * s <= picMax.y - picMin.y;
        });
        const Real s = 1.0 / denominator;
        ProductViewOptions view;
        view.dimensions = false;
        place(canvas, drawProductView(in.product, picture, view), s, picMin, picMax);
        canvas.text(Vec2((picMin.x + picMax.x) / 2.0, picMin.y - 2.0),
                    txt(picture == ProductView::Plan ? TextId::ViewPlan : TextId::ViewFrontClosed)
                        + "  " + scaleText(denominator),
                    kBody, false, HAlign::Center, VAlign::Top);
    }
}

// --- Edge banding and hardware ----------------------------------------------------------------

void hardwarePage(Composer& composer, const DocumentationInput& in)
{
    Canvas canvas = composer.newPage(txt(TextId::PageHardware), txt(TextId::PageHardware));
    const Vec2 min = composer.contentMin();
    const Vec2 max = composer.contentMax();
    const Real width = std::min(200.0, max.x - min.x);
    Real y = max.y;

    canvas.text(Vec2(min.x, y), txt(TextId::EdgeUsage), kBody, true, HAlign::Left, VAlign::Top);
    y -= kBody + 2.5;
    Table bands;
    bands.columns = {{txt(TextId::ColEdgeBand), 2.0},
                     {txt(TextId::Thickness), 1.0, HAlign::Right},
                     {txt(TextId::ColPieces), 1.0, HAlign::Right},
                     {txt(TextId::ColLength), 1.0, HAlign::Right}};
    for (const detailing::EdgeBandUsage& u : in.edgeUsage) {
        bands.rows.push_back({{u.band.name, mm(u.band.thickness) + " mm", std::to_string(u.pieces),
                               twoDecimals(u.totalLength / 1000.0)}});
    }
    y -= drawTable(canvas, Vec2(min.x, y), width, y - min.y, bands, 0).second + kGap;

    canvas.text(Vec2(min.x, y), txt(TextId::HardwareList), kBody, true, HAlign::Left, VAlign::Top);
    y -= kBody + 2.5;
    Table hardware;
    hardware.columns = {{txt(TextId::ColNo), 0.4, HAlign::Right},
                        {txt(TextId::ColItem), 2.2},
                        {txt(TextId::ColSpec), 2.0},
                        {txt(TextId::ColQuantity), 0.8, HAlign::Right}};
    const auto items = detailing::hardwareList(in.product);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const detailing::HardwareItem& item = items[i];
        std::string spec = item.spec;
        if (!item.article.empty()) {
            spec += " (" + item.article + ")";
        }
        hardware.rows.push_back(
            {{std::to_string(i + 1), txt(i18n::hardwareText(item.kind)), spec, std::to_string(item.quantity)}});
    }
    drawTable(canvas, Vec2(min.x, y), width, y - min.y, hardware, 0);
}

// --- Assembly drawing -------------------------------------------------------------------------

/// The three views of the assembly drawing, arranged in rows. In first-angle projection the
/// views of one row stand on the same floor line.
using ViewRows = std::vector<std::vector<std::size_t>>;

struct ViewSet {
    std::array<Drawing, 3> drawings;
    std::array<Bounds, 3> bounds; // drawing units

    /// Size on paper of one row and of all rows (with titles and gaps).
    [[nodiscard]] Vec2 rowSize(const std::vector<std::size_t>& row, Real s, Real titleSpace) const
    {
        Real width = 0.0;
        Real above = 0.0;
        Real below = 0.0;
        for (const std::size_t i : row) {
            width += bounds[i].size().x * s;
            above = std::max(above, bounds[i].max.y * s);
            below = std::max(below, -bounds[i].min.y * s);
        }
        return {width + 2.0 * kGap * static_cast<Real>(row.size() - 1), above + below + titleSpace};
    }
    [[nodiscard]] Vec2 size(const ViewRows& rows, Real s, Real titleSpace) const
    {
        Vec2 total(0.0);
        for (const auto& row : rows) {
            const Vec2 r = rowSize(row, s, titleSpace);
            total.x = std::max(total.x, r.x);
            total.y += r.y;
        }
        total.y += kGap * static_cast<Real>(rows.size() - 1);
        return total;
    }
};

/// The plan of a corner kitchen: all its walls from above, as large as the page allows.
void planPage(Composer& composer, const DocumentationInput& in, const TextMetrics& metrics)
{
    const Vec2 area = composer.contentSize();
    const auto build = [&](Real scale) {
        ProductViewOptions options;
        options.textHeight = kBody / scale;
        return drawProductView(in.product, ProductView::Plan, options);
    };
    const int denominator = chooseScale([&](Real s) {
        const Bounds b = build(s).bounds(metrics);
        return b.size().x * s <= area.x && b.size().y * s <= area.y;
    });
    const Real s = 1.0 / denominator;
    const Drawing plan = build(s);
    const Bounds b = plan.bounds(metrics);

    Canvas canvas = composer.newPage(txt(TextId::PagePlan), txt(TextId::PagePlan));
    composer.setScale(scaleText(denominator));
    const Vec2 size = b.size() * s;
    const Vec2 offset(composer.contentMin().x + (area.x - size.x) / 2.0 - b.min.x * s,
                      composer.contentMax().y - (area.y - size.y) / 2.0 - b.max.y * s);
    canvas.drawing().append(plan, s, offset);
}

/// The assembly drawing of `root`: the product, or one wall of a corner kitchen.
void assemblyPage(Composer& composer, const TextMetrics& metrics, const core::FurnitureComponent& root,
                  const std::string& title)
{
    const std::array<ProductView, 3> views{ProductView::Front, ProductView::FrontOpen,
                                           ProductView::Section};
    const std::array<TextId, 3> titles{TextId::ViewFrontClosed, TextId::ViewFrontOpen,
                                       TextId::ViewSection};
    const Vec2 area = composer.contentSize();
    constexpr Real kTitleSpace = 8.0;

    const auto build = [&](Real scale) {
        ViewSet set;
        ProductViewOptions options;
        options.textHeight = kBody / scale;
        for (std::size_t i = 0; i < views.size(); ++i) {
            set.drawings[i] = drawProductView(root, views[i], options);
            set.bounds[i] = set.drawings[i].bounds(metrics);
        }
        return set;
    };
    // One row on wide pages; two rows (the fronts above, the section below) when that allows
    // a larger scale, e.g. on A4 portrait.
    const std::array<ViewRows, 2> layouts{ViewRows{{0, 1, 2}}, ViewRows{{0, 1}, {2}}};
    int denominator = 0;
    const ViewRows* layout = nullptr;
    for (const ViewRows& candidate : layouts) {
        const int d = chooseScale([&](Real s) {
            const Vec2 needed = build(s).size(candidate, s, kTitleSpace);
            return needed.x <= area.x && needed.y <= area.y;
        });
        if (layout == nullptr || d < denominator) {
            denominator = d;
            layout = &candidate;
        }
    }
    const Real s = 1.0 / denominator;
    const ViewSet set = build(s);

    Canvas canvas = composer.newPage(title, title);
    composer.setScale(scaleText(denominator));

    const Vec2 total = set.size(*layout, s, kTitleSpace);
    Real rowTop = composer.contentMax().y - std::max(0.0, (area.y - total.y) / 2.0);
    for (const auto& row : *layout) {
        const Vec2 rowSize = set.rowSize(row, s, kTitleSpace);
        Real above = 0.0;
        for (const std::size_t i : row) {
            above = std::max(above, set.bounds[i].max.y * s);
        }
        const Real floorY = rowTop - above;
        Real x = composer.contentMin().x + std::max(0.0, (area.x - rowSize.x) / 2.0);
        for (const std::size_t i : row) {
            const Bounds& b = set.bounds[i];
            const Vec2 offset(x - b.min.x * s, floorY);
            canvas.drawing().append(set.drawings[i], s, offset);
            const Bounds extent = viewExtent(root, views[i]);
            canvas.text(Vec2(offset.x + extent.center().x * s, rowTop - rowSize.y + kTitleSpace - 2.0),
                        txt(titles[i]), kBody + 0.5, true, HAlign::Center, VAlign::Top);
            x += b.size().x * s + 2.0 * kGap;
        }
        rowTop -= rowSize.y + kGap;
    }
}

// --- Part drawings ----------------------------------------------------------------------------

std::string faceName(core::PanelFace face)
{
    return txt(face == core::PanelFace::Top ? TextId::FaceA : TextId::FaceB);
}

Table holeTable(const detailing::FlatPart& part)
{
    using detailing::FlatSide;
    Table table;
    table.textHeight = 2.0;
    table.columns = {{txt(TextId::ColNo), 0.7, HAlign::Right},
                     {txt(TextId::ColSide), 1.5},
                     {"X", 1.3, HAlign::Right},
                     {"Y", 1.3, HAlign::Right},
                     {"Z", 0.9, HAlign::Right},
                     {txt(TextId::ColDiameter), 0.9, HAlign::Right},
                     {txt(TextId::ColDepth), 1.3, HAlign::Right},
                     {txt(TextId::ColType), 1.8}};
    const core::PanelFace other = part.viewFace == core::PanelFace::Top ? core::PanelFace::Bottom
                                                                        : core::PanelFace::Top;
    std::size_t n = 0;
    for (const detailing::FlatHole& h : part.holes) {
        std::string side;
        switch (h.side) {
        case FlatSide::Near: side = faceName(part.viewFace); break;
        case FlatSide::Far:  side = faceName(other); break;
        case FlatSide::Edge:
            side = txt(TextId::SideEdge) + " " + std::string(i18n::text(i18n::edgeText(h.edge)));
            break;
        }
        table.rows.push_back({{std::to_string(++n), side, mm(h.center.x), mm(h.center.y),
                               h.side == FlatSide::Edge ? mm(h.height) : "—", mm(h.diameter),
                               mm(h.depth), std::string(i18n::text(i18n::holeText(h.fastener)))}});
    }
    for (const detailing::FlatGroove& g : part.grooves) {
        table.rows.push_back(
            {{std::to_string(++n), faceName(g.side == FlatSide::Near ? part.viewFace : other),
              mm(g.start.x) + "–" + mm(g.end.x), mm(g.start.y) + "–" + mm(g.end.y), "—",
              mm(g.width), mm(g.depth), txt(TextId::HoleGroove)}});
    }
    return table;
}

void partPages(Composer& composer, const DocumentationInput& in, std::size_t number,
               const detailing::CutListRow& row, const TextMetrics& metrics)
{
    const detailing::FlatPart part = detailing::flatPartOf(in.product, row);
    const std::string title = std::to_string(number) + ". " + partName(row);
    const Vec2 min = composer.contentMin();
    const Vec2 max = composer.contentMax();

    // Landscape-like pages: info on the right; narrow pages: info below the drawing.
    const bool side = max.x - min.x >= 230.0;
    const Real infoWidth = side ? 100.0 : max.x - min.x;
    const Vec2 drawMin = side ? min : Vec2(min.x, min.y + (max.y - min.y) * 0.5 + kGap);
    const Vec2 drawMax = side ? Vec2(max.x - infoWidth - kGap, max.y) : max;

    const auto build = [&](Real scale) {
        PartDrawingOptions options;
        options.textHeight = kBody / scale;
        return drawPart(part, options);
    };
    const int denominator = chooseScale([&](Real s) {
        const Vec2 size = build(s).bounds(metrics).size() * s;
        return size.x <= drawMax.x - drawMin.x && size.y <= drawMax.y - drawMin.y;
    });

    Canvas canvas = composer.newPage(txt(TextId::PagePart) + ": " + title, title);
    composer.setScale(scaleText(denominator));
    // The drawing at the top of its area; whatever stays free below it takes the hole table.
    const Real s = 1.0 / denominator;
    const Drawing drawing = build(s);
    const Bounds b = drawing.bounds(metrics);
    const Vec2 offset((drawMin.x + drawMax.x) / 2.0 - b.center().x * s, drawMax.y - b.max.y * s);
    canvas.drawing().append(drawing, s, offset);
    const Real drawingBottom = offset.y + b.min.y * s;
    const Vec2 infoTopLeft = side ? Vec2(max.x - infoWidth, max.y)
                                  : Vec2(min.x, drawingBottom - kGap);

    // Facts about the part.
    std::string edgesLong;
    std::string edgesShort;
    for (const core::EdgeSide e : {core::EdgeSide::L1, core::EdgeSide::L2, core::EdgeSide::W1,
                                   core::EdgeSide::W2}) {
        const core::EdgeBand& band = part.edge(e).band;
        std::string& target = (e == core::EdgeSide::L1 || e == core::EdgeSide::L2) ? edgesLong
                                                                                 : edgesShort;
        if (!target.empty()) {
            target += "   ";
        }
        target += std::string(i18n::text(i18n::edgeText(e))) + ": "
                + (band.present() ? band.name : "—");
    }
    Table facts;
    facts.header = false;
    facts.textHeight = 2.0;
    facts.columns = {{"", 1.0}, {"", 2.0}};
    facts.rows = {
        {{txt(TextId::ColPart), title}, true},
        {{txt(TextId::ColQuantity), std::to_string(row.quantity)}},
        {{txt(TextId::ColMaterial), row.material.name}},
        {{txt(TextId::FinishSize), sizeText(row.finishLength, row.finishWidth) + " mm"}},
        {{txt(TextId::CutSize), sizeText(row.cut.length, row.cut.width) + " mm"}},
        {{txt(TextId::EdgeBanding), edgesLong}},
        {{"", edgesShort}},
        {{txt(TextId::Grain), txt(row.keepGrainDirection ? TextId::GrainKept : TextId::GrainFree)}},
        {{txt(TextId::ViewFrom), faceName(part.viewFace)}},
    };
    // Notches, shaped corners and openings the user added, named by the edges of the drawing.
    if (const core::FurnitureComponent* c = in.product.find(row.components.front())) {
        bool first = true;
        for (const core::ShapeFeature& feature : c->panel()->shape()) {
            facts.rows.push_back({{first ? txt(TextId::ShapeHeader) : "", i18n::featureText(feature)}});
            first = false;
        }
    }
    Real y = infoTopLeft.y;
    y -= drawTable(canvas, infoTopLeft, infoWidth, y - min.y, facts, 0).second + kGap;

    const Table holes = holeTable(part);
    if (holes.rows.empty()) {
        return;
    }
    canvas.text(Vec2(infoTopLeft.x, y), txt(TextId::HoleTable), kBody, true, HAlign::Left,
                VAlign::Top);
    y -= kBody + 2.5;
    canvas.text(Vec2(infoTopLeft.x, min.y - 2.5), txt(TextId::OriginNote), kSmall, false,
                HAlign::Left, VAlign::Top, true);
    std::size_t next = drawTable(canvas, Vec2(infoTopLeft.x, y), infoWidth, y - min.y, holes, 0).first;

    // Continue under the drawing if there is room for a few rows ...
    const Real below = drawingBottom - kGap - min.y;
    const Real rowHeight = kRow * holes.textHeight / kTable;
    if (side && next < holes.rows.size() && below >= 4.0 * rowHeight) {
        next = drawTable(canvas, Vec2(drawMin.x, drawingBottom - kGap),
                         std::min(drawMax.x - drawMin.x, infoWidth * 1.3), below, holes, next)
                   .first;
    }
    // ... and on continuation pages after that.
    while (next < holes.rows.size()) {
        Canvas more = composer.newPage(txt(TextId::PagePart) + ": " + title + " "
                                           + txt(TextId::Continued),
                                       {});
        composer.setScale(scaleText(denominator));
        next = drawTable(more, Vec2(min.x, max.y), std::min(max.x - min.x, 160.0), max.y - min.y,
                         holes, next)
                   .first;
    }
}

// --- Cutting plan -----------------------------------------------------------------------------

std::string machineName(nesting::Machine machine)
{
    return txt(machine == nesting::Machine::PanelSaw ? TextId::MachinePanelSaw
                                                     : TextId::MachineNestingCenter);
}

std::string sourceName(nesting::SheetSource source)
{
    switch (source) {
    case nesting::SheetSource::Remnant:  return txt(TextId::FromRemnant);
    case nesting::SheetSource::Stock:    return txt(TextId::FromStock);
    case nesting::SheetSource::Purchase: return txt(TextId::ToPurchase);
    }
    return {};
}

/// "%78": the Turkish way of writing a percentage.
std::string percent(Real fraction)
{
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%%%.0f", fraction * 100.0);
    return buffer;
}

/// The machine settings the layouts depend on, in one line.
std::string machineSettings(const nesting::NestingSettings& s)
{
    if (s.machine == nesting::Machine::PanelSaw) {
        return txt(TextId::Kerf) + " " + mm(s.saw.kerf) + " mm · " + txt(TextId::Trim) + " "
             + mm(s.saw.trim) + " mm · " + txt(TextId::Stages) + " "
             + std::to_string(s.saw.maxStages);
    }
    return txt(TextId::ToolDiameter) + " " + mm(s.router.toolDiameter) + " mm · "
         + txt(TextId::SheetMargin) + " " + mm(s.router.margin) + " mm";
}

/// Overview: machine, sheets per material, every layout. A long list continues on more pages.
void cuttingOverview(Composer& composer, const DocumentationInput& in, const PartSource& source)
{
    const nesting::NestingResult& r = *in.cutting;
    Canvas canvas = composer.newPage(txt(TextId::PageCutting), txt(TextId::PageCutting));
    const Vec2 min = composer.contentMin();
    const Vec2 max = composer.contentMax();
    const Real width = std::min(220.0, max.x - min.x);
    Real y = max.y;

    Table facts;
    facts.header = false;
    facts.columns = {{"", 1.0}, {"", 2.6}};
    facts.rows = {
        {{txt(TextId::Machine), machineName(r.settings.machine)}, true},
        {{"", machineSettings(r.settings)}},
        {{txt(TextId::SheetsTotal), std::to_string(r.sheetCount())}},
        {{txt(TextId::Utilization), percent(r.utilization())}},
    };
    if (!r.settings.useRemnants) {
        facts.rows.push_back({{"", txt(TextId::RemnantsNotUsed)}});
    }
    y -= drawTable(canvas, Vec2(min.x, y), std::min(width, 150.0), y - min.y, facts, 0).second
       + kGap;

    Table materials;
    materials.columns = {{txt(TextId::ColMaterial), 2.4},
                         {txt(TextId::ColSheetSize), 1.8, HAlign::Right},
                         {txt(TextId::FromStock), 1.1, HAlign::Right},
                         {txt(TextId::FromRemnant), 1.1, HAlign::Right},
                         {txt(TextId::ToPurchase), 1.4, HAlign::Right},
                         {txt(TextId::Utilization), 1.1, HAlign::Right},
                         {txt(TextId::NewRemnants), 1.1, HAlign::Right}};
    for (const nesting::MaterialUsage& u : r.usage()) {
        std::string size = "—";
        for (const nesting::SheetLayout& l : r.layouts) {
            if (nesting::sameMaterial(l.sheet.material, u.material)
                && l.source != nesting::SheetSource::Remnant) {
                size = sizeText(l.sheet.length, l.sheet.width);
                break;
            }
        }
        materials.rows.push_back({{u.material.name, size, std::to_string(u.stock),
                                   std::to_string(u.remnants), std::to_string(u.purchase),
                                   percent(u.utilization()), std::to_string(u.newRemnants)}});
    }
    y -= drawTable(canvas, Vec2(min.x, y), width, y - min.y, materials, 0).second + kGap;

    if (!r.unplaced.empty()) {
        canvas.text(Vec2(min.x, y), txt(TextId::Unplaced), kBody, true, HAlign::Left, VAlign::Top);
        y -= kBody + 2.5;
        for (const nesting::NestingResult::Unplaced& u : r.unplaced) {
            const nesting::NestingPart& p = r.parts[u.part];
            const nesting::Rect box = nesting::bounds(p.outline);
            canvas.text(Vec2(min.x, y),
                        "• " + p.label + ". " + placedPartName(r, u.part, &source) + "  "
                            + sizeText(box.width(), box.height()) + "  ×"
                            + std::to_string(u.quantity),
                        kTable, false, HAlign::Left, VAlign::Top);
            y -= kTable * 1.9;
        }
        y -= kGap - kTable;
    }

    Table sheets;
    sheets.columns = {{txt(TextId::ColNo), 0.6, HAlign::Right},
                      {txt(TextId::ColMaterial), 2.4},
                      {txt(TextId::ColSheetSize), 1.8, HAlign::Right},
                      {txt(TextId::ColQuantity), 0.9, HAlign::Right},
                      {txt(TextId::PartsOnSheet), 1.5, HAlign::Right},
                      {txt(TextId::Utilization), 1.1, HAlign::Right},
                      {txt(TextId::ColSource), 1.5}};
    for (std::size_t i = 0; i < r.layouts.size(); ++i) {
        const nesting::SheetLayout& l = r.layouts[i];
        sheets.rows.push_back({{std::to_string(i + 1), l.sheet.material.name,
                                sizeText(l.sheet.length, l.sheet.width), std::to_string(l.count),
                                std::to_string(l.parts.size()), percent(l.utilization()),
                                sourceName(l.source)}});
    }
    std::size_t next = 0;
    if (y - min.y > 4.0 * kRow) {
        next = drawTable(canvas, Vec2(min.x, y), width, y - min.y, sheets, 0).first;
    }
    while (next < sheets.rows.size()) {
        Canvas more = composer.newPage(txt(TextId::PageCutting) + " " + txt(TextId::Continued), {});
        next = drawTable(more, Vec2(min.x, max.y), width, max.y - min.y, sheets, next).first;
    }
}

/// One sheet layout: the cutting map as large as it fits, its facts and the parts on it.
void cuttingSheetPage(Composer& composer, const DocumentationInput& in, const PartSource& source,
                      std::size_t index)
{
    const nesting::NestingResult& r = *in.cutting;
    const nesting::SheetLayout& l = r.layouts[index];
    const Vec2 min = composer.contentMin();
    const Vec2 max = composer.contentMax();
    const bool side = max.x - min.x >= 230.0;
    const Real infoWidth = side ? 78.0 : max.x - min.x;
    const Vec2 drawMin = side ? min : Vec2(min.x, min.y + (max.y - min.y) * 0.4 + kGap);
    const Vec2 drawMax = side ? Vec2(max.x - infoWidth - kGap, max.y) : max;

    // Labels stay inside the sheet: the sheet itself decides the scale.
    const int denominator = chooseScale([&](Real s) {
        return l.sheet.length * s <= drawMax.x - drawMin.x
            && l.sheet.width * s + 4.0 <= drawMax.y - drawMin.y;
    });
    const Real s = 1.0 / denominator;
    SheetDrawingOptions options;
    options.textHeight = kTable / s;
    options.partDetails = false;
    options.fills = true;
    const Drawing map = drawSheetLayout(r, index, &source, options);

    const std::string title = sheetTitle(r, index);
    Canvas canvas = composer.newPage(txt(TextId::PageCutting) + ": " + title,
                                     txt(TextId::SheetLabel) + " " + std::to_string(index + 1));
    composer.setScale(scaleText(denominator));
    const Vec2 offset((drawMin.x + drawMax.x) / 2.0 - l.sheet.length * s / 2.0,
                      drawMax.y - l.sheet.width * s);
    canvas.drawing().append(map, s, offset);
    if (r.settings.machine == nesting::Machine::PanelSaw) {
        canvas.text(Vec2(offset.x, offset.y - 1.5), txt(TextId::CutsNote), kSmall, false,
                    HAlign::Left, VAlign::Top, true);
    }

    const Vec2 infoTopLeft = side ? Vec2(max.x - infoWidth, max.y)
                                  : Vec2(min.x, offset.y - kGap - 3.0);
    std::string remnants;
    for (const nesting::Rect& rest : l.remnants) {
        remnants += (remnants.empty() ? "" : ", ") + sizeText(rest.width(), rest.height());
    }
    Table facts;
    facts.header = false;
    facts.textHeight = 2.0;
    facts.columns = {{"", 1.0}, {"", 1.6}};
    facts.rows = {
        {{txt(TextId::ColMaterial), l.sheet.material.name}, true},
        {{txt(TextId::ColSheetSize), sizeText(l.sheet.length, l.sheet.width) + " mm"}},
        {{txt(TextId::ColQuantity), std::to_string(l.count) + "  (" + sourceName(l.source) + ")"}},
        {{txt(TextId::Utilization), percent(l.utilization())}},
        {{txt(TextId::Remnant), remnants.empty() ? "—" : remnants}},
    };
    if (r.settings.machine == nesting::Machine::PanelSaw) {
        const int perStack = nesting::sheetsPerStack(r.settings.saw, l.sheet.material.thickness);
        const int stacks = (l.count + perStack - 1) / perStack;
        facts.rows.push_back(
            {{txt(TextId::StackCount), std::to_string(stacks)}});
    }
    Real y = infoTopLeft.y;
    y -= drawTable(canvas, infoTopLeft, infoWidth, y - min.y, facts, 0).second + kGap;

    // The parts of one sheet, counted.
    std::vector<std::pair<std::size_t, int>> counts;
    for (const nesting::PlacedPart& p : l.parts) {
        const auto it = std::find_if(counts.begin(), counts.end(),
                                     [&](const auto& c) { return c.first == p.part; });
        if (it == counts.end()) {
            counts.emplace_back(p.part, 1);
        } else {
            ++it->second;
        }
    }
    std::sort(counts.begin(), counts.end()); // in cut list order
    Table parts;
    parts.textHeight = 2.0;
    parts.columns = {{txt(TextId::ColNo), 0.6, HAlign::Right},
                     {txt(TextId::ColPart), 2.0},
                     {txt(TextId::ColCut), 1.7, HAlign::Right},
                     {txt(TextId::ColQuantity), 0.7, HAlign::Right}};
    for (const auto& [part, count] : counts) {
        const nesting::Rect box = nesting::bounds(r.parts[part].outline);
        parts.rows.push_back({{r.parts[part].label, placedPartName(r, part, &source),
                               sizeText(box.width(), box.height()), std::to_string(count)}});
    }
    canvas.text(Vec2(infoTopLeft.x, y), txt(TextId::PartsOnSheet), kBody, true, HAlign::Left,
                VAlign::Top);
    y -= kBody + 2.5;
    std::size_t next =
        drawTable(canvas, Vec2(infoTopLeft.x, y), infoWidth, y - min.y, parts, 0).first;
    while (next < parts.rows.size()) {
        Canvas more = composer.newPage(txt(TextId::PageCutting) + ": " + title + " "
                                           + txt(TextId::Continued),
                                       {});
        composer.setScale(scaleText(denominator));
        next = drawTable(more, Vec2(min.x, max.y), std::min(max.x - min.x, 160.0), max.y - min.y,
                         parts, next)
                   .first;
    }
}

void cuttingPages(Composer& composer, const DocumentationInput& in)
{
    const PartSource source{in.product, in.cutList};
    cuttingOverview(composer, in, source);
    for (std::size_t i = 0; i < in.cutting->layouts.size(); ++i) {
        cuttingSheetPage(composer, in, source, i);
    }
}

} // namespace

// ---------------------------------------------------------------------------------------------

int chooseScale(const std::function<bool(Real scale)>& fits)
{
    static constexpr std::array<int, 10> kDenominators{1, 2, 5, 10, 15, 20, 25, 50, 100, 200};
    for (const int d : kDenominators) {
        if (fits(1.0 / d)) {
            return d;
        }
    }
    return kDenominators.back();
}

std::string scaleText(int denominator)
{
    return "1:" + std::to_string(denominator);
}

PageSet buildDocumentation(const DocumentationInput& in, const DocumentationOptions& options,
                           const TextMetrics& metrics)
{
    std::string product = options.productName;
    if (product.empty()) {
        product = in.product.name().empty() ? txt(TextId::Untitled) : in.product.name();
    }
    const core::Aabb bounds = core::panelBounds(in.product);
    Composer composer(options, product, sizeText(bounds.size().x, bounds.size().y, bounds.size().z),
                      metrics);

    if (options.summary) {
        summaryPage(composer, in, product);
    }
    if (options.cutList) {
        const bool outOfPlan = std::any_of(in.cutList.begin(), in.cutList.end(),
                                           [](const auto& r) { return !r.inCuttingPlan; });
        paginate(composer, txt(TextId::PageCutList), txt(TextId::PageCutList),
                 cutListTable(in.cutList), outOfPlan ? txt(TextId::NotInCuttingPlanNote) : "");
    }
    if (options.hardware) {
        hardwarePage(composer, in);
    }
    if (options.cuttingPlan && in.cutting != nullptr && !in.cutting->layouts.empty()) {
        cuttingPages(composer, in);
    }
    if (options.assembly) {
        // A corner kitchen: its plan, then each wall as seen standing in front of it.
        if (cornerKitchen(in.product)) {
            planPage(composer, in, metrics);
            for (const core::KitchenRun* run : in.product.kitchen()->runs()) {
                assemblyPage(composer, metrics, *run,
                             txt(TextId::PageAssembly) + " — " + i18n::displayName(*run));
            }
        } else {
            assemblyPage(composer, metrics, in.product, txt(TextId::PageAssembly));
        }
    }
    if (options.partDrawings) {
        for (std::size_t i = 0; i < in.cutList.size(); ++i) {
            partPages(composer, in, i + 1, in.cutList[i], metrics);
        }
    }
    return composer.finish(product + " — " + txt(TextId::ReportTitle));
}

void saveDocumentationPdf(const DocumentationInput& input, const DocumentationOptions& options,
                          const std::filesystem::path& path, const PdfFonts& fonts)
{
    const PdfFontMetrics metrics(fonts);
    savePdf(buildDocumentation(input, options, metrics), path, fonts);
}

} // namespace rc::exporting
