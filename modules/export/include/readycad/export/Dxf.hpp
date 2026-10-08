#pragma once

#include "readycad/export/Drawing.hpp"
#include "readycad/i18n/Utf8.hpp"

#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace rc::exporting {

/// Thrown when a file cannot be written.
class ExportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Writes the drawing as DXF R12 (AC1009), units mm.
///
/// Why R12: it is the version every CAD/CAM program reads — AutoCAD, LibreCAD, QCAD, nesting
/// and CNC software of machine makers — and it needs no object handles, so the file stays
/// simple and readable. Structure: HEADER (version, extents) -> TABLES (LTYPE, LAYER, STYLE)
/// -> BLOCKS (empty) -> ENTITIES -> EOF.
///
/// Mapping: Line -> LINE, Circle -> CIRCLE, Polyline -> POLYLINE/VERTEX/SEQEND (filled
/// triangles and quads -> SOLID), Text -> TEXT, Dimension -> its lines, SOLID arrows and TEXT.
/// Non-ASCII letters (Turkish ı, ş, ğ ...) are written as \U+XXXX, which AutoCAD and LibreCAD
/// read regardless of the code page; '⌀' becomes the AutoCAD code %%c.
void writeDxf(const Drawing& drawing, std::ostream& out);

[[nodiscard]] std::string toDxf(const Drawing& drawing);

/// Throws ExportError.
void saveDxf(const Drawing& drawing, const std::filesystem::path& path);

/// "Raf 2" -> "RAF_2": what DXF R12 allows in a name (A-Z, 0-9, _, -, $); Turkish letters are
/// replaced by their Latin base letter.
[[nodiscard]] std::string dxfName(std::string_view name);

/// Turkish letters to ASCII ("Çekmece yanı" -> "Cekmece yani"): for file names that CNC
/// software and USB sticks of machines can handle. Lives in i18n, shared with the CAM module.
using i18n::asciiFold;

} // namespace rc::exporting
