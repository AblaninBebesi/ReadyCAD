#include "readycad/export/Dxf.hpp"

#include "Utf8.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <ostream>
#include <sstream>

namespace rc::exporting {

namespace {

/// Locale-independent number: up to 6 decimals, no trailing zeros, no "-0".
std::string number(Real value)
{
    if (std::abs(value) < 5e-7) {
        return "0";
    }
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value,
                                      std::chars_format::fixed, 6);
    std::string text(buffer, result.ptr);
    if (text.find('.') != std::string::npos) {
        while (text.back() == '0') {
            text.pop_back();
        }
        if (text.back() == '.') {
            text.pop_back();
        }
    }
    return text == "-0" ? "0" : text;
}

/// DXF text value: ASCII, other characters as \U+XXXX, the diameter sign as %%c.
std::string dxfText(std::string_view utf8)
{
    std::string out;
    for (const char32_t c : decodeUtf8(utf8)) {
        if (c == U'⌀' || c == U'∅') {
            out += "%%c";
        } else if (c == U'\n' || c == U'\r') {
            out += ' ';
        } else if (c < 0x80) {
            out += static_cast<char>(c);
        } else {
            char buffer[16];
            std::snprintf(buffer, sizeof buffer, "\\U+%04X", static_cast<unsigned>(c));
            out += buffer;
        }
    }
    return out;
}

const char* lineTypeName(LineType type) noexcept
{
    switch (type) {
    case LineType::Continuous: return "CONTINUOUS";
    case LineType::Dashed:     return "DASHED";
    case LineType::Center:     return "CENTER";
    }
    return "CONTINUOUS";
}

class DxfWriter {
public:
    explicit DxfWriter(std::ostream& out) : out_(out) {}

    void pair(int code, std::string_view value)
    {
        char buffer[8];
        std::snprintf(buffer, sizeof buffer, "%3d", code);
        out_ << buffer << '\n' << value << '\n';
    }
    void pair(int code, Real value) { pair(code, number(value)); }
    void integer(int code, int value) { pair(code, std::to_string(value)); }
    void point(int code, Vec2 p)
    {
        pair(code, p.x);
        pair(code + 10, p.y);
        pair(code + 20, 0.0);
    }

    void header(const Bounds& bounds)
    {
        const Vec2 min = bounds.empty ? Vec2(0.0) : bounds.min;
        const Vec2 max = bounds.empty ? Vec2(0.0) : bounds.max;
        section("HEADER");
        variable("$ACADVER");
        pair(1, "AC1009");
        variable("$DWGCODEPAGE");
        pair(3, "ANSI_1252");
        variable("$INSBASE");
        point(10, Vec2(0.0));
        variable("$EXTMIN");
        point(10, min);
        variable("$EXTMAX");
        point(10, max);
        variable("$LIMMIN");
        pair(10, min.x);
        pair(20, min.y);
        variable("$LIMMAX");
        pair(10, max.x);
        pair(20, max.y);
        variable("$LTSCALE");
        pair(40, 1.0);
        variable("$TEXTSTYLE");
        pair(7, "STANDARD");
        variable("$CLAYER");
        pair(8, "0");
        endSection();
    }

    void tables(const std::vector<Layer>& layers)
    {
        section("TABLES");

        beginTable("LTYPE", 3);
        lineType("CONTINUOUS", "Solid line", {});
        lineType("DASHED", "Dashed __ __ __", {6.0, -3.0});
        lineType("CENTER", "Center ____ _ ____", {20.0, -4.0, 2.0, -4.0});
        endTable();

        const bool hasLayer0 = std::any_of(layers.begin(), layers.end(),
                                           [](const Layer& l) { return dxfName(l.name) == "0"; });
        beginTable("LAYER", static_cast<int>(layers.size()) + (hasLayer0 ? 0 : 1));
        if (!hasLayer0) {
            layer("0", 7, LineType::Continuous);
        }
        for (const Layer& l : layers) {
            layer(dxfName(l.name), l.aci, l.lineType);
        }
        endTable();

        beginTable("STYLE", 1);
        pair(0, "STYLE");
        pair(2, "STANDARD");
        integer(70, 0);
        pair(40, 0.0); // variable height: every TEXT gives its own
        pair(41, 1.0);
        pair(50, 0.0);
        integer(71, 0);
        pair(42, 2.5);
        pair(3, "arial.ttf");
        pair(4, "");
        endTable();

        endSection();
    }

    void blocks()
    {
        section("BLOCKS");
        endSection();
    }

    void entity(const std::string& layer, const Shape& shape)
    {
        std::visit([&](const auto& e) { write(layer, e); }, shape);
    }

    void section(std::string_view name)
    {
        pair(0, "SECTION");
        pair(2, name);
    }
    void endSection() { pair(0, "ENDSEC"); }
    void end() { pair(0, "EOF"); }

private:
    void variable(std::string_view name) { pair(9, name); }

    void beginTable(std::string_view name, int count)
    {
        pair(0, "TABLE");
        pair(2, name);
        integer(70, count);
    }
    void endTable() { pair(0, "ENDTAB"); }

    void lineType(std::string_view name, std::string_view description,
                  const std::vector<Real>& pattern)
    {
        Real total = 0.0;
        for (const Real dash : pattern) {
            total += std::abs(dash);
        }
        pair(0, "LTYPE");
        pair(2, name);
        integer(70, 0);
        pair(3, description);
        integer(72, 65); // 'A': alignment code
        integer(73, static_cast<int>(pattern.size()));
        pair(40, total);
        for (const Real dash : pattern) {
            pair(49, dash);
        }
    }

    void layer(const std::string& name, int aci, LineType type)
    {
        pair(0, "LAYER");
        pair(2, name);
        integer(70, 0);
        integer(62, aci);
        pair(6, lineTypeName(type));
    }

    void write(const std::string& layer, const Line& line)
    {
        pair(0, "LINE");
        pair(8, layer);
        point(10, line.a);
        point(11, line.b);
    }

    void write(const std::string& layer, const Circle& circle)
    {
        pair(0, "CIRCLE");
        pair(8, layer);
        point(10, circle.center);
        pair(40, circle.radius);
    }

    void write(const std::string& layer, const Polyline& poly)
    {
        const std::size_t n = poly.points.size();
        if (poly.filled && (n == 3 || n == 4)) {
            // SOLID corners go 1-2-4-3 ("Z" order): swap the last two of a quad.
            const Vec2 p3 = n == 3 ? poly.points[2] : poly.points[3];
            const Vec2 p4 = poly.points[2];
            pair(0, "SOLID");
            pair(8, layer);
            point(10, poly.points[0]);
            point(11, poly.points[1]);
            point(12, p3);
            point(13, p4);
            return;
        }
        if (n < 2) {
            return;
        }
        pair(0, "POLYLINE");
        pair(8, layer);
        integer(66, 1); // vertices follow
        point(10, Vec2(0.0));
        integer(70, poly.closed ? 1 : 0);
        for (const Vec2& p : poly.points) {
            pair(0, "VERTEX");
            pair(8, layer);
            point(10, p);
        }
        pair(0, "SEQEND");
        pair(8, layer);
    }

    void write(const std::string& layer, const Text& text)
    {
        if (text.text.empty()) {
            return;
        }
        pair(0, "TEXT");
        pair(8, layer);
        point(10, text.position);
        pair(40, text.height);
        pair(1, dxfText(text.text));
        if (std::abs(text.angle) > 1e-9) {
            pair(50, text.angle);
        }
        pair(7, "STANDARD");
        const int h = text.hAlign == HAlign::Left ? 0 : text.hAlign == HAlign::Center ? 1 : 2;
        int v = 0;
        switch (text.vAlign) {
        case VAlign::Baseline: v = 0; break;
        case VAlign::Bottom:   v = 1; break;
        case VAlign::Middle:   v = 2; break;
        case VAlign::Top:      v = 3; break;
        }
        if (h != 0 || v != 0) {
            integer(72, h);
            point(11, text.position); // with an alignment, this point is the one that counts
            integer(73, v);
        }
    }

    void write(const std::string& layer, const Dimension& dimension)
    {
        for (const Shape& part : explode(dimension, metrics_)) {
            entity(layer, part);
        }
    }

    std::ostream& out_;
    ApproxTextMetrics metrics_;
};

} // namespace

std::string asciiFold(std::string_view text)
{
    std::string out;
    for (const char32_t c : decodeUtf8(text)) {
        switch (c) {
        case U'ç': out += 'c'; break;
        case U'Ç': out += 'C'; break;
        case U'ğ': out += 'g'; break;
        case U'Ğ': out += 'G'; break;
        case U'ı': out += 'i'; break;
        case U'İ': out += 'I'; break;
        case U'ö': out += 'o'; break;
        case U'Ö': out += 'O'; break;
        case U'ş': out += 's'; break;
        case U'Ş': out += 'S'; break;
        case U'ü': out += 'u'; break;
        case U'Ü': out += 'U'; break;
        case U'â': out += 'a'; break;
        case U'î': out += 'i'; break;
        case U'û': out += 'u'; break;
        default:   out += c < 0x80 ? static_cast<char>(c) : '_'; break;
        }
    }
    return out;
}

std::string dxfName(std::string_view name)
{
    std::string out;
    for (const char c : asciiFold(name)) {
        if (c >= 'a' && c <= 'z') {
            out += static_cast<char>(c - 'a' + 'A');
        } else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'
                   || c == '$') {
            out += c;
        } else {
            out += '_';
        }
    }
    if (out.empty()) {
        out = "0";
    }
    return out.substr(0, 31); // R12 limit
}

void writeDxf(const Drawing& drawing, std::ostream& out)
{
    DxfWriter dxf(out);
    dxf.header(drawing.bounds(ApproxTextMetrics{}));
    dxf.tables(drawing.layers());
    dxf.blocks();
    dxf.section("ENTITIES");
    std::vector<std::string> names;
    for (const Layer& layer : drawing.layers()) {
        names.push_back(dxfName(layer.name));
    }
    for (const Entity& e : drawing.entities()) {
        dxf.entity(names[e.layer], e.shape);
    }
    dxf.endSection();
    dxf.end();
}

std::string toDxf(const Drawing& drawing)
{
    std::ostringstream out;
    writeDxf(drawing, out);
    return out.str();
}

void saveDxf(const Drawing& drawing, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw ExportError("cannot write " + pathToUtf8(path));
    }
    writeDxf(drawing, file);
    file.flush();
    if (!file) {
        throw ExportError("cannot write " + pathToUtf8(path));
    }
}

} // namespace rc::exporting
