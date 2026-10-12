#pragma once

#include "readycad/cam/NestingCenter.hpp"
#include "readycad/editor/Document.hpp"
#include "readycad/export/Documentation.hpp"
#include "readycad/export/Pdf.hpp"
#include "readycad/nesting/Nesting.hpp"
#include "readycad/nesting/Stock.hpp"

#include <filesystem>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

/// Stage 9: the production pipeline — ONE path from a designed product to the files of the
/// workshop. The menus, the command line and the tests all go through it, so what the tests
/// check is exactly what the user gets:
///
///     Document (product + drilling + cut list, kept up to date by the editor)
///       -> cuttingParts()       what the workshop cuts from its sheets
///       -> planCutting()        sheets, remnants, saw cuts (nesting)
///       -> write...()           PDF documents, DXF drawings, CNC programs
///       -> writePackage()       all of it in one folder, with a summary for the workshop
namespace rc::production {

/// A request the pipeline cannot fulfil: a stale cutting plan, a folder that is not empty...
class ProductionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// What the workshop is asked to make: `quantity` copies of the document's product.
struct Order {
    const editor::Document& document;
    int quantity = 1;
};

/// Name of the order in documents and file names: the project file's name, else the product's
/// name, else "Adsız".
[[nodiscard]] std::string orderName(const editor::Document& document);

/// The parts the workshop cuts from its sheets: every cut list row `quantity` times, without
/// the parts left out of the cutting plan. Throws std::invalid_argument for a quantity < 1.
[[nodiscard]] std::vector<nesting::NestingPart> cuttingParts(const Order& order);

/// The cutting plan of the order, computed in the calling thread. (The UI computes the same in
/// the background: nesting::NestingRun over cuttingParts().)
[[nodiscard]] nesting::NestingResult planCutting(const Order& order,
                                                 const nesting::StockManager& stock,
                                                 const nesting::NestingSettings& settings,
                                                 const nesting::ProgressCallback& progress = {},
                                                 std::stop_token stop = {});

/// True if `plan` was made for the order as it is now. Files made from a stale plan would cut
/// parts that no longer exist, so every writer below refuses such a plan (ProductionError).
[[nodiscard]] bool planMatches(const Order& order, const nesting::NestingResult& plan);

// --- Single outputs ---------------------------------------------------------------------------
// Each writes one kind of file; the menus use them one by one, writePackage() all together.

/// The PDF documents; with the cutting plan pages if `plan` is given. An empty product name in
/// `options` becomes orderName().
void writeDocumentation(const Order& order, const nesting::NestingResult* plan,
                        exporting::DocumentationOptions options, const exporting::PdfFonts& fonts,
                        const std::filesystem::path& file);

/// All parts on one DXF drawing.
void writePartsDxf(const Order& order, const std::filesystem::path& file);

/// One DXF per cut list row in `folder` (created if missing), for CAM software.
std::vector<std::filesystem::path> writePartDxfs(const Order& order,
                                                 const std::filesystem::path& folder);

/// One DXF per sheet layout of the plan in `folder`.
std::vector<std::filesystem::path> writeSheetDxfs(const Order& order,
                                                  const nesting::NestingResult& plan,
                                                  const std::filesystem::path& folder);

/// One CNC program per sheet of a nesting centre plan in `folder`. Everything is planned before
/// the first file is written. `programs` receives the planned programs (times, work left for
/// other machines). Throws cam::CamError for a panel saw plan or a profile that cannot cut it.
std::vector<std::filesystem::path> writeCncPrograms(const Order& order,
                                                    const nesting::NestingResult& plan,
                                                    const cam::NestingCenterProfile& profile,
                                                    const std::filesystem::path& folder,
                                                    std::vector<cam::SheetProgram>* programs = nullptr);

/// The cut list as CSV for the panel saw's own optimiser (or a spreadsheet): one line per row,
/// quantities for the whole order, "Kesim planında" yes/no. Semicolons and decimal commas, UTF-8
/// with BOM — what a Turkish Excel opens without asking.
void writeCutListCsv(const Order& order, const std::filesystem::path& file);

// --- The production package -------------------------------------------------------------------

struct PackageOptions {
    exporting::DocumentationOptions documentation;
    bool documents = true;     ///< <name>.pdf
    bool partDrawings = true;  ///< DXF/: one drawing per part
    bool sheetDrawings = true; ///< KESIM/: one drawing per sheet (with a plan)
    bool cncPrograms = true;   ///< CNC/: one program per sheet (with a nesting centre plan)
    bool cutListCsv = true;    ///< <name>_kesim_listesi.csv
    bool project = true;       ///< <name>.rcproj: the design the files were made from
};

/// What writePackage() wrote.
struct Package {
    std::filesystem::path folder;
    std::string date;                ///< of the documents: "12.10.2026"
    std::filesystem::path documents; ///< empty if not written
    std::filesystem::path cutListCsv;
    std::filesystem::path project;
    std::vector<std::filesystem::path> partDrawings;
    std::vector<std::filesystem::path> sheetDrawings;
    std::vector<std::filesystem::path> programs;
    std::vector<cam::SheetProgram> cnc; ///< the planned programs, in the order of `programs`
    std::filesystem::path summary;      ///< OZET.txt
    std::string summaryText;            ///< its content
};

/// Everything the workshop needs for the order, in one folder:
///
///     <name>.pdf                 documents (with the cutting plan pages)
///     <name>_kesim_listesi.csv   cut list for the saw's optimiser
///     <name>.rcproj              the design itself
///     DXF/                       one drawing per part
///     KESIM/                     one drawing per sheet
///     CNC/                       one program per sheet (nesting centre)
///     OZET.txt                   what is in the package: sheets, times, work left for other
///                                machines, parts that are not cut here
///
/// `plan` may be null (no cutting plan files then) but must match the order. `folder` must not
/// exist or be empty: an old program for a sheet that is no longer in the plan must never lie
/// next to the new ones. All or nothing: if anything fails, nothing is left in the folder.
/// Throws ProductionError, or the error of the writer that failed.
[[nodiscard]] Package writePackage(const Order& order, const nesting::NestingResult* plan,
                                   const cam::NestingCenterProfile& profile,
                                   const PackageOptions& options,
                                   const exporting::PdfFonts& fonts,
                                   const std::filesystem::path& folder);

/// A folder in `parent` that does not exist yet, for the package of an order named `name`:
/// "Gardirop_URETIM", then "Gardirop_URETIM_2", "Gardirop_URETIM_3"...
[[nodiscard]] std::filesystem::path newPackageFolder(const std::filesystem::path& parent,
                                                     const std::string& name);

} // namespace rc::production
