#pragma once

#include "readycad/cam/Program.hpp"

#include <memory>
#include <string>
#include <vector>

namespace rc::cam {

/// Turns a Program into the text file one kind of controller reads. A new machine brand or
/// controller = one more Postprocessor; the operations and the planning stay as they are.
class Postprocessor {
public:
    virtual ~Postprocessor() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    /// Without the dot: "nc".
    [[nodiscard]] virtual std::string fileExtension() const = 0;
    [[nodiscard]] virtual std::string write(const Program& program) const = 0;
};

// ---------------------------------------------------------------------------------------------
// ISO G-code and its dialects
// ---------------------------------------------------------------------------------------------

/// Where Z = 0 of the controller is.
enum class ZeroPlane {
    Table, ///< on the spoilboard: the sheet's top is at Z = thickness (usual for nesting)
    Top,   ///< on the sheet's top: cuts go to negative Z
};

enum class CommentStyle {
    Parentheses, ///< (comment) — ISO
    Semicolon,   ///< ; comment
};

/// What differs between controllers that all speak ISO G-code (Fanuc style): the lines around
/// the program, tool changes, drill cycles, number format. Templates may use {name},
/// {length}, {width}, {thickness} (program), {tool}, {rpm}, {diameter} (tool changes).
struct GCodeDialect {
    std::string name = "ISO G-code";
    std::string extension = "nc";
    int decimals = 3;
    bool lineNumbers = false; ///< N10, N20, ...
    int lineNumberStep = 10;
    CommentStyle comments = CommentStyle::Parentheses;
    bool percentLines = true; ///< "%" as the first and the last line
    ZeroPlane zero = ZeroPlane::Table;
    std::vector<std::string> header{"G21 G90 G17", "G54"};
    std::vector<std::string> footer{"M5", "G0 X0 Y0", "M30"};
    std::vector<std::string> toolChange{"T{tool} M6"};
    std::vector<std::string> spindleOn{"S{rpm} M3"};
    bool drillCycles = true; ///< G81 ... G80; false: every hole as G0 / G1 / G0 moves

    friend bool operator==(const GCodeDialect&, const GCodeDialect&) = default;
};

/// Plain ISO G-code: G81 drill cycles, Z = 0 on the table.
[[nodiscard]] GCodeDialect isoDialect();
/// Starting values for KDT nesting centres (NcStudio-style controllers): line numbers, M06 tool
/// changes, holes as explicit moves (works on controllers without canned cycles). The tool
/// numbers and the lines around the program have to be checked against the real machine.
[[nodiscard]] GCodeDialect kdtDialect();
[[nodiscard]] std::vector<GCodeDialect> dialectPresets();

class GCodePostprocessor final : public Postprocessor {
public:
    explicit GCodePostprocessor(GCodeDialect dialect) : dialect_(std::move(dialect)) {}

    [[nodiscard]] std::string name() const override { return dialect_.name; }
    [[nodiscard]] std::string fileExtension() const override { return dialect_.extension; }
    [[nodiscard]] std::string write(const Program& program) const override;

private:
    GCodeDialect dialect_;
};

} // namespace rc::cam
