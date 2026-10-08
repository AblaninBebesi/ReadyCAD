#include "readycad/cam/Postprocessor.hpp"

#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Utf8.hpp"

#include <charconv>
#include <cmath>
#include <optional>
#include <utility>

namespace rc::cam {

GCodeDialect isoDialect()
{
    return GCodeDialect{};
}

GCodeDialect kdtDialect()
{
    GCodeDialect d;
    d.name = "KDT (NcStudio)";
    d.extension = "nc";
    d.lineNumbers = true;
    d.percentLines = false;
    d.header = {"G90 G54 G17 G21"};
    d.footer = {"M05", "G0 X0 Y0", "M30"};
    d.toolChange = {"M05", "T{tool} M06"};
    d.spindleOn = {"S{rpm} M03"};
    d.drillCycles = false;
    return d;
}

std::vector<GCodeDialect> dialectPresets()
{
    return {isoDialect(), kdtDialect()};
}

namespace {

constexpr Real kEps = 1e-6;

void replaceAll(std::string& text, std::string_view key, const std::string& value)
{
    for (std::size_t at = text.find(key); at != std::string::npos;
         at = text.find(key, at + value.size())) {
        text.replace(at, key.size(), value);
    }
}

/// Builds the text line by line: numbers, comments, coordinates in one format.
class Writer {
public:
    explicit Writer(const GCodeDialect& dialect) : d_(dialect) {}

    void raw(std::string_view text)
    {
        out_ += text;
        out_ += '\n';
    }

    void line(const std::string& text)
    {
        if (d_.lineNumbers) {
            number_ += d_.lineNumberStep;
            out_ += 'N' + std::to_string(number_) + ' ';
        }
        raw(text);
    }

    /// ASCII only, without the characters that would end the comment early.
    void comment(std::string_view text)
    {
        std::string clean;
        for (const char c : i18n::asciiFold(text)) {
            clean += (c == '(' || c == ')' || c == ';') ? ' ' : c;
        }
        line(d_.comments == CommentStyle::Parentheses ? "(" + clean + ")" : "; " + clean);
    }

    /// Fixed decimals, '.' whatever the locale, never "-0.000".
    [[nodiscard]] std::string number(Real value) const
    {
        const Real scale = std::pow(10.0, d_.decimals);
        Real rounded = std::round(value * scale) / scale;
        if (rounded == 0.0) {
            rounded = 0.0; // drops the sign of -0
        }
        char buffer[48];
        const auto result = std::to_chars(buffer, buffer + sizeof buffer, rounded,
                                          std::chars_format::fixed, d_.decimals);
        return std::string(buffer, result.ptr);
    }

    [[nodiscard]] std::string take() { return std::move(out_); }

private:
    const GCodeDialect& d_;
    std::string out_;
    int number_ = 0;
};

class GCodeWriter {
public:
    GCodeWriter(const GCodeDialect& dialect, const Program& program)
        : d_(dialect), p_(program), w_(dialect)
        , zShift_(dialect.zero == ZeroPlane::Top ? program.thickness : 0.0)
    {
    }

    std::string run()
    {
        if (d_.percentLines) {
            w_.raw("%");
        }
        w_.comment("ReadyCAD " + p_.name);
        w_.comment(i18n::mm(p_.length) + " x " + i18n::mm(p_.width) + " x "
                   + i18n::mm(p_.thickness) + " mm, Z0 = "
                   + (d_.zero == ZeroPlane::Table ? "TABLE" : "TOP"));
        for (const std::string& h : d_.header) {
            w_.line(fill(h, nullptr));
        }
        for (const Step& step : p_.steps) {
            std::visit([&](const auto& s) { write(s); }, step);
        }
        endCycle();
        for (const std::string& f : d_.footer) {
            w_.line(fill(f, nullptr));
        }
        if (d_.percentLines) {
            w_.raw("%");
        }
        return w_.take();
    }

private:
    [[nodiscard]] std::string fill(std::string text, const Tool* tool) const
    {
        replaceAll(text, "{name}", p_.name);
        replaceAll(text, "{length}", w_.number(p_.length));
        replaceAll(text, "{width}", w_.number(p_.width));
        replaceAll(text, "{thickness}", w_.number(p_.thickness));
        if (tool != nullptr) {
            replaceAll(text, "{tool}", std::to_string(tool->number));
            replaceAll(text, "{rpm}", std::to_string(tool->rpm));
            replaceAll(text, "{diameter}", w_.number(tool->diameter));
        }
        return text;
    }

    [[nodiscard]] Real z(Real workpieceZ) const noexcept { return workpieceZ - zShift_; }

    [[nodiscard]] std::string axis(char name, Real value) const
    {
        return std::string(1, name) + w_.number(value);
    }

    void endCycle()
    {
        if (inCycle_) {
            w_.line("G80");
            inCycle_ = false;
        }
    }

    void write(const Comment& c) { w_.comment(c.text); }

    void write(const ToolChange& c)
    {
        endCycle();
        for (const std::string& t : d_.toolChange) {
            w_.line(fill(t, &c.tool));
        }
        for (const std::string& t : d_.spindleOn) {
            w_.line(fill(t, &c.tool));
        }
        w_.comment(c.tool.name.empty() ? "T" + std::to_string(c.tool.number) : c.tool.name);
        feed_.reset();
    }

    /// Lift first, travel, descend last: the tool never travels low over the sheet.
    void write(const Rapid& r)
    {
        endCycle();
        const Real tz = z(r.to.z);
        const bool xyChanged = !at_ || std::abs(at_->x - r.to.x) > kEps
                            || std::abs(at_->y - r.to.y) > kEps;
        const bool up = !at_ || tz > at_->z + kEps;
        const bool down = at_ && tz < at_->z - kEps;
        if (up) {
            w_.line("G0 " + axis('Z', tz));
        }
        if (xyChanged) {
            w_.line("G0 " + axis('X', r.to.x) + " " + axis('Y', r.to.y));
        }
        if (down) {
            w_.line("G0 " + axis('Z', tz));
        }
        at_ = Vec3(r.to.x, r.to.y, tz);
    }

    void write(const Feed& f)
    {
        endCycle();
        const Vec3 to(f.to.x, f.to.y, z(f.to.z));
        std::string text = "G1";
        if (!at_ || std::abs(at_->x - to.x) > kEps) {
            text += " " + axis('X', to.x);
        }
        if (!at_ || std::abs(at_->y - to.y) > kEps) {
            text += " " + axis('Y', to.y);
        }
        if (!at_ || std::abs(at_->z - to.z) > kEps) {
            text += " " + axis('Z', to.z);
        }
        if (!feed_ || std::abs(*feed_ - f.feed) > kEps) {
            text += " F" + std::to_string(std::lround(f.feed));
            feed_ = f.feed;
        }
        if (text != "G1") {
            w_.line(text);
        }
        at_ = to;
    }

    void write(const DrillCycle& c)
    {
        const Real r = z(c.clearance);
        const Real bottom = z(c.bottom);
        if (d_.drillCycles) {
            // G99: back to the R plane after each hole; the next holes only need X and Y.
            const bool same = inCycle_ && std::abs(cycleR_ - r) < kEps
                           && std::abs(cycleZ_ - bottom) < kEps && std::abs(cycleF_ - c.feed) < kEps;
            if (same) {
                w_.line(axis('X', c.at.x) + " " + axis('Y', c.at.y));
            } else {
                endCycle();
                w_.line("G99 G81 " + axis('X', c.at.x) + " " + axis('Y', c.at.y) + " "
                        + axis('Z', bottom) + " " + axis('R', r) + " F"
                        + std::to_string(std::lround(c.feed)));
                inCycle_ = true;
                cycleR_ = r;
                cycleZ_ = bottom;
                cycleF_ = c.feed;
            }
            at_ = Vec3(c.at.x, c.at.y, r);
            feed_.reset();
            return;
        }
        // The same as explicit moves, for controllers without canned cycles.
        if (!at_ || std::abs(at_->x - c.at.x) > kEps || std::abs(at_->y - c.at.y) > kEps) {
            w_.line("G0 " + axis('X', c.at.x) + " " + axis('Y', c.at.y));
        }
        if (!at_ || std::abs(at_->z - r) > kEps) {
            w_.line("G0 " + axis('Z', r));
        }
        w_.line("G1 " + axis('Z', bottom) + " F" + std::to_string(std::lround(c.feed)));
        w_.line("G0 " + axis('Z', r));
        at_ = Vec3(c.at.x, c.at.y, r);
        feed_ = c.feed;
    }

    const GCodeDialect& d_;
    const Program& p_;
    Writer w_;
    Real zShift_;
    std::optional<Vec3> at_; // machine coordinates
    std::optional<Real> feed_;
    bool inCycle_ = false;
    Real cycleR_ = 0.0;
    Real cycleZ_ = 0.0;
    Real cycleF_ = 0.0;
};

} // namespace

std::string GCodePostprocessor::write(const Program& program) const
{
    return GCodeWriter(dialect_, program).run();
}

} // namespace rc::cam
