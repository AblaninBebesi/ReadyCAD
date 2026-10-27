#include "readycad/production/Pipeline.hpp"

#include "readycad/cam/Postprocessor.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/SheetDrawing.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/i18n/Utf8.hpp"
#include "readycad/production/Summary.hpp"
#include "readycad/storage/Storage.hpp"

#include <algorithm>
#include <system_error>

namespace rc::production {

using i18n::TextId;

namespace {

detailing::PartSource sourceOf(const Order& order)
{
    return {order.document.product(), order.document.cutList()};
}

void requireMatching(const Order& order, const nesting::NestingResult& plan)
{
    if (!planMatches(order, plan)) {
        throw ProductionError("The cutting plan was made for another state of the product; "
                              "compute it again");
    }
}

// --- CSV ------------------------------------------------------------------------------------

/// A Turkish or Russian spreadsheet reads a decimal comma and cells separated by ';', an English
/// one a decimal point and ','.
bool commaDecimals()
{
    return i18n::currentLanguage() != i18n::Language::English;
}

/// A cell as Excel reads it: quoted if it contains the separator, a quote or a line break.
std::string csvCell(const std::string& value)
{
    const char* special = commaDecimals() ? ";\"\r\n" : ",\"\r\n";
    if (value.find_first_of(special) == std::string::npos) {
        return value;
    }
    std::string quoted = "\"";
    for (const char c : value) {
        quoted += c == '"' ? std::string("\"\"") : std::string(1, c);
    }
    return quoted + "\"";
}

/// 558.5 -> "558,5" in a Turkish or Russian spreadsheet.
std::string csvNumber(core::Real value)
{
    std::string text = i18n::mm(value);
    if (commaDecimals()) {
        std::replace(text.begin(), text.end(), '.', ',');
    }
    return text;
}

std::string csvLine(const std::vector<std::string>& cells)
{
    std::string line;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        line += (i > 0 ? (commaDecimals() ? ";" : ",") : "") + csvCell(cells[i]);
    }
    return line + "\r\n";
}

std::string txt(TextId id)
{
    return std::string(i18n::text(id));
}

/// Plain text for people: a BOM (old Notepad reads it as UTF-8) and Windows line breaks.
std::string forNotepad(const std::string& content)
{
    std::string out = "\xEF\xBB\xBF";
    for (const char c : content) {
        if (c == '\n') {
            out += "\r\n";
        } else {
            out += c;
        }
    }
    return out;
}

} // namespace

std::string orderName(const editor::Document& document)
{
    if (!document.path().empty()) {
        return i18n::pathToUtf8(document.path().stem());
    }
    if (!document.product().name().empty()) {
        return document.product().name();
    }
    return txt(TextId::Untitled);
}

std::vector<nesting::NestingPart> cuttingParts(const Order& order)
{
    return nesting::partsFromCutList(order.document.product(), order.document.cutList(),
                                     order.quantity);
}

nesting::NestingResult planCutting(const Order& order, const nesting::StockManager& stock,
                                   const nesting::NestingSettings& settings,
                                   const nesting::ProgressCallback& progress, std::stop_token stop)
{
    return nesting::optimize(cuttingParts(order), stock, settings, progress, std::move(stop));
}

bool planMatches(const Order& order, const nesting::NestingResult& plan)
{
    if (order.quantity < 1) {
        return false;
    }
    return plan.parts == cuttingParts(order);
}

// ---------------------------------------------------------------------------------------------
// Single outputs
// ---------------------------------------------------------------------------------------------

void writeDocumentation(const Order& order, const nesting::NestingResult* plan,
                        exporting::DocumentationOptions options, const exporting::PdfFonts& fonts,
                        const std::filesystem::path& file)
{
    if (plan != nullptr) {
        requireMatching(order, *plan);
    }
    const editor::Document& doc = order.document;
    // The documents prefer the product's own name ("Yatak odası dolabı"); a product without
    // one is called after its file.
    if (options.productName.empty() && doc.product().name().empty() && !doc.path().empty()) {
        options.productName = i18n::pathToUtf8(doc.path().stem());
    }
    exporting::DocumentationInput input{doc.product(), doc.cutList(), doc.edgeUsage(),
                                        doc.detailingReport()};
    input.cutting = plan;
    exporting::saveDocumentationPdf(input, options, file, fonts);
}

void writePartsDxf(const Order& order, const std::filesystem::path& file)
{
    exporting::saveDxf(exporting::drawAllParts(order.document.product(), order.document.cutList()),
                       file);
}

std::vector<std::filesystem::path> writePartDxfs(const Order& order,
                                                 const std::filesystem::path& folder)
{
    return exporting::savePartDxfs(order.document.product(), order.document.cutList(), folder);
}

std::vector<std::filesystem::path> writeSheetDxfs(const Order& order,
                                                  const nesting::NestingResult& plan,
                                                  const std::filesystem::path& folder)
{
    requireMatching(order, plan);
    const detailing::PartSource source = sourceOf(order);
    return exporting::saveSheetDxfs(plan, &source, folder);
}

std::vector<std::filesystem::path> writeCncPrograms(const Order& order,
                                                    const nesting::NestingResult& plan,
                                                    const cam::NestingCenterProfile& profile,
                                                    const std::filesystem::path& folder,
                                                    std::vector<cam::SheetProgram>* programs)
{
    requireMatching(order, plan);
    const detailing::PartSource source = sourceOf(order);
    const cam::GCodePostprocessor post(profile.dialect);
    return cam::saveSheetPrograms(plan, &source, profile, post, folder, programs);
}

void writeCutListCsv(const Order& order, const std::filesystem::path& file)
{
    std::string csv = "\xEF\xBB\xBF"; // Excel: UTF-8
    csv += csvLine({txt(TextId::ColNo), txt(TextId::ColPart), txt(TextId::ColMaterial),
                    txt(TextId::Thickness), txt(TextId::ColCut) + " " + txt(TextId::Length),
                    txt(TextId::ColCut) + " " + txt(TextId::Width),
                    txt(TextId::ColFinish) + " " + txt(TextId::Length),
                    txt(TextId::ColFinish) + " " + txt(TextId::Width),
                    txt(TextId::ColQuantity), txt(TextId::ColGrain), txt(TextId::EdgeL1),
                    txt(TextId::EdgeL2), txt(TextId::EdgeW1), txt(TextId::EdgeW2),
                    txt(TextId::InCuttingPlan)});
    const auto& cutList = order.document.cutList();
    for (std::size_t i = 0; i < cutList.size(); ++i) {
        const detailing::CutListRow& row = cutList[i];
        std::vector<std::string> cells{
            std::to_string(i + 1),
            exporting::partName(row),
            row.material.name,
            csvNumber(row.thickness),
            csvNumber(row.cut.length),
            csvNumber(row.cut.width),
            csvNumber(row.finishLength),
            csvNumber(row.finishWidth),
            std::to_string(row.quantity * order.quantity),
            txt(row.keepGrainDirection ? TextId::GrainKept : TextId::GrainFree),
        };
        for (const core::EdgeBand& band : row.edges) {
            cells.push_back(band.present() ? band.name : std::string());
        }
        cells.push_back(txt(row.inCuttingPlan ? TextId::Yes : TextId::No));
        csv += csvLine(cells);
    }
    storage::writeTextFile(file, csv);
}

// ---------------------------------------------------------------------------------------------
// Package
// ---------------------------------------------------------------------------------------------

Package writePackage(const Order& order, const nesting::NestingResult* plan,
                     const cam::NestingCenterProfile& profile, const PackageOptions& options,
                     const exporting::PdfFonts& fonts, const std::filesystem::path& folder)
{
    if (plan != nullptr) {
        requireMatching(order, *plan);
    }
    std::error_code ec;
    const bool existed = std::filesystem::exists(folder, ec);
    if (existed
        && (!std::filesystem::is_directory(folder, ec) || !std::filesystem::is_empty(folder, ec))) {
        throw ProductionError("The folder of the package is not empty: " + i18n::pathToUtf8(folder));
    }
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        throw ProductionError("Cannot create the folder " + i18n::pathToUtf8(folder) + ": "
                              + ec.message());
    }

    Package package;
    package.folder = folder;
    package.date = options.documentation.date.empty() ? exporting::todayText()
                                                      : options.documentation.date;
    const std::string name = i18n::safeFileName(orderName(order.document));
    try {
        // The CNC programs first: every sheet is planned before a file is written, so a machine
        // that cannot cut the plan (router wider than the spacing) stops the package early.
        const bool nestingCenter =
            plan != nullptr && plan->settings.machine == nesting::Machine::NestingCenter;
        if (options.cncPrograms && nestingCenter && !plan->layouts.empty()) {
            package.programs = writeCncPrograms(order, *plan, profile, folder / "CNC", &package.cnc);
        }
        if (options.documents) {
            package.documents = folder / (name + ".pdf");
            writeDocumentation(order, plan, options.documentation, fonts, package.documents);
        }
        if (options.cutListCsv) {
            package.cutListCsv = folder / (name + "_" + txt(TextId::FileCutList) + ".csv");
            writeCutListCsv(order, package.cutListCsv);
        }
        if (options.project) {
            package.project = folder / (name + ".rcproj");
            storage::writeTextFile(package.project,
                                   storage::saveProduct(order.document.product()));
        }
        if (options.partDrawings) {
            package.partDrawings = writePartDxfs(order, folder / "DXF");
        }
        if (options.sheetDrawings && plan != nullptr && !plan->layouts.empty()) {
            package.sheetDrawings = writeSheetDxfs(order, *plan, folder / txt(TextId::FolderCutting));
        }
        package.summary = folder / (txt(TextId::FileSummary) + ".txt");
        package.summaryText = summaryText(order, plan, package);
        storage::writeTextFile(package.summary, forNotepad(package.summaryText));
    } catch (...) {
        // All or nothing. The folder was empty, so everything in it now was written here.
        std::error_code ignored;
        if (existed) {
            for (const auto& entry : std::filesystem::directory_iterator(folder, ignored)) {
                std::filesystem::remove_all(entry.path(), ignored);
            }
        } else {
            std::filesystem::remove_all(folder, ignored);
        }
        throw;
    }
    return package;
}

std::filesystem::path newPackageFolder(const std::filesystem::path& parent, const std::string& name)
{
    const std::string base = i18n::safeFileName(name) + "_" + txt(TextId::FolderProduction);
    std::filesystem::path candidate = parent / base;
    std::error_code ec;
    for (int n = 2; std::filesystem::exists(candidate, ec); ++n) {
        candidate = parent / (base + "_" + std::to_string(n));
    }
    return candidate;
}

} // namespace rc::production
