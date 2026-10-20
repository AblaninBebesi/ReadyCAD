#include "readycad/production/Summary.hpp"

#include "readycad/core/model/Queries.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/SheetDrawing.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/i18n/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rc::production {

using i18n::TextId;

namespace {

std::string txt(TextId id)
{
    return std::string(i18n::text(id));
}

/// Width on screen of a UTF-8 text (code points: "ı" is one character, two bytes).
std::size_t widthOf(const std::string& text)
{
    return i18n::decodeUtf8(text).size();
}

/// Columns aligned with spaces, numbers to the right: a table readable in any text editor.
std::string textTable(const std::vector<std::vector<std::string>>& rows,
                      const std::vector<bool>& rightAligned)
{
    std::vector<std::size_t> widths;
    for (const auto& row : rows) {
        widths.resize(std::max(widths.size(), row.size()), 0);
        for (std::size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], widthOf(row[i]));
        }
    }
    std::string out;
    for (const auto& row : rows) {
        std::string line;
        for (std::size_t i = 0; i < row.size(); ++i) {
            const std::string padding(widths[i] - widthOf(row[i]), ' ');
            const bool right = i < rightAligned.size() && rightAligned[i];
            line += (i > 0 ? "  " : "") + (right ? padding + row[i] : row[i] + padding);
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        out += line + "\n";
    }
    return out;
}

/// "Kesim planı" and a line under it.
std::string heading(const std::string& title)
{
    return "\n" + title + "\n" + std::string(widthOf(title), '-') + "\n";
}

std::string percent(double fraction)
{
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%%%.0f", fraction * 100.0);
    return buffer;
}

std::string sizeText(core::Real a, core::Real b)
{
    return i18n::mm(a) + " × " + i18n::mm(b);
}

/// A path inside the package as the summary names it: "CNC/01_Suntalam_18.nc".
std::string relative(const Package& package, const std::filesystem::path& file)
{
    const std::u8string text = file.lexically_relative(package.folder).generic_u8string();
    return {text.begin(), text.end()};
}

} // namespace

std::string durationText(double seconds)
{
    const long total = std::lround(std::max(0.0, seconds));
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%ld %s %02ld %s", total / 60, i18n::tr(TextId::Minutes),
                  total % 60, i18n::tr(TextId::SecondsShort));
    return buffer;
}

std::string leftoverText(const std::vector<cam::SheetProgram>& programs)
{
    cam::Leftover sum;
    for (const cam::SheetProgram& p : programs) {
        for (const cam::Leftover& l : p.leftovers) {
            sum.edgeHoles += l.edgeHoles;
            sum.bottomHoles += l.bottomHoles + l.bottomGrooves;
            sum.noTool += l.noTool;
        }
    }
    std::string text;
    const auto add = [&](int n, TextId what) {
        if (n > 0) {
            text += (text.empty() ? "" : "\n") + std::to_string(n) + " " + txt(what);
        }
    };
    add(sum.edgeHoles, TextId::LeftoverEdge);
    add(sum.bottomHoles, TextId::LeftoverBottom);
    add(sum.noTool, TextId::LeftoverNoTool);
    return text;
}

double totalMachiningSeconds(const nesting::NestingResult& plan,
                             const std::vector<cam::SheetProgram>& programs)
{
    double seconds = 0.0;
    for (std::size_t i = 0; i < programs.size() && i < plan.layouts.size(); ++i) {
        seconds += programs[i].stats.seconds * plan.layouts[i].count;
    }
    return seconds;
}

std::vector<std::string> attentionLines(const editor::Document& document)
{
    std::vector<std::string> lines;
    for (const core::Overlap& o : document.overlaps()) {
        lines.push_back(txt(TextId::OverlapWarning) + ": " + i18n::displayName(*o.first) + " — "
                        + i18n::displayName(*o.second));
    }
    for (const core::Carcass* cabinet : document.misplacedCutouts()) {
        lines.push_back(txt(TextId::CutoutWarning) + ": " + i18n::displayName(*cabinet));
    }
    for (const core::Worktop* worktop : document.longWorktops()) {
        lines.push_back(txt(TextId::LongWorktopWarning) + ": " + i18n::displayName(*worktop) + " "
                        + i18n::mm(worktop->box().size().x) + " mm");
    }
    for (const detailing::ShapeConflict& c : document.shapeConflicts()) {
        lines.push_back(txt(TextId::ShapeWarning) + ": " + i18n::displayName(*c.part));
    }
    for (const std::string& warning : document.detailingReport().warnings) {
        lines.push_back(warning);
    }
    return lines;
}

std::string summaryText(const Order& order, const nesting::NestingResult* plan,
                        const Package& package)
{
    const editor::Document& doc = order.document;
    const auto& cutList = doc.cutList();

    std::string out = txt(TextId::PackageTitle) + "\n";
    out += std::string(widthOf(txt(TextId::PackageTitle)), '=') + "\n\n";

    // --- The order --------------------------------------------------------------------------
    int parts = 0;
    int outOfPlan = 0;
    for (const detailing::CutListRow& row : cutList) {
        parts += row.quantity * order.quantity;
        outOfPlan += row.inCuttingPlan ? 0 : row.quantity * order.quantity;
    }
    std::string orderText = orderName(doc);
    if (order.quantity > 1) {
        orderText += " × " + std::to_string(order.quantity);
    }
    const core::Vec3 size = core::panelBounds(doc.product()).size();
    std::string partsText = std::to_string(parts);
    if (outOfPlan > 0) {
        partsText += " (" + std::to_string(parts - outOfPlan) + " " + txt(TextId::InPlanShort)
                   + ", " + std::to_string(outOfPlan) + " " + txt(TextId::OutOfPlanShort) + ")";
    }
    out += textTable({{txt(TextId::SummaryOrder), ":", orderText},
                      {txt(TextId::SummaryDate), ":", package.date},
                      {txt(TextId::SummarySize), ":",
                       sizeText(size.x, size.y) + " × " + i18n::mm(size.z) + " mm"},
                      {txt(TextId::SummaryParts), ":", partsText}},
                     {});

    // --- Before anything else: what must be checked -----------------------------------------
    if (const auto attention = attentionLines(doc); !attention.empty()) {
        out += heading(txt(TextId::SummaryAttention));
        for (const std::string& line : attention) {
            out += "! " + line + "\n";
        }
    }

    // --- The cutting plan -------------------------------------------------------------------
    if (plan == nullptr) {
        out += heading(txt(TextId::SummaryCutting));
        out += txt(TextId::NoCuttingPlan) + "\n";
    } else {
        const bool saw = plan->settings.machine == nesting::Machine::PanelSaw;
        out += heading(txt(TextId::SummaryCutting) + " — "
                       + txt(saw ? TextId::MachinePanelSaw : TextId::MachineNestingCenter));
        std::vector<std::vector<std::string>> rows{
            {txt(TextId::ColMaterial), txt(TextId::SheetsTotal), txt(TextId::FromStock),
             txt(TextId::FromRemnant), txt(TextId::ToPurchase), txt(TextId::Utilization),
             txt(TextId::NewRemnants)}};
        for (const nesting::MaterialUsage& u : plan->usage()) {
            rows.push_back({u.material.name, std::to_string(u.sheets()), std::to_string(u.stock),
                            std::to_string(u.remnants), std::to_string(u.purchase),
                            percent(u.utilization()), std::to_string(u.newRemnants)});
        }
        out += textTable(rows, {false, true, true, true, true, true, true});
        if (!plan->settings.useRemnants) {
            out += txt(TextId::RemnantsNotUsed) + ".\n";
        }
        if (!plan->unplaced.empty()) {
            const detailing::PartSource source{doc.product(), cutList};
            out += "\n" + txt(TextId::Unplaced) + ":\n";
            for (const nesting::NestingResult::Unplaced& u : plan->unplaced) {
                out += "  " + plan->parts[u.part].label + "  "
                     + exporting::placedPartName(*plan, u.part, &source) + "  × "
                     + std::to_string(u.quantity) + "\n";
            }
        }
        if (saw) {
            out += "\n" + txt(TextId::SawProgramsNote) + "\n";
            // The saw cuts rectangles: notches, shaped corners and openings come after it.
            std::vector<std::vector<std::string>> shaped;
            for (std::size_t i = 0; i < cutList.size(); ++i) {
                const detailing::CutListRow& row = cutList[i];
                if (!row.inCuttingPlan) {
                    continue;
                }
                const detailing::FlatPart flat = detailing::flatPartOf(doc.product(), row);
                if (flat.shaped() || !flat.cutouts.empty()) {
                    shaped.push_back({std::to_string(i + 1), exporting::partName(row),
                                      "× " + std::to_string(row.quantity * order.quantity)});
                }
            }
            if (!shaped.empty()) {
                out += heading(txt(TextId::AfterSawTitle));
                out += textTable(shaped, {true, false, true});
                out += txt(TextId::AfterSawNote) + "\n";
            }
        }
    }

    // --- CNC programs -----------------------------------------------------------------------
    if (!package.programs.empty() && plan != nullptr) {
        out += heading(txt(TextId::SummaryCnc));
        std::vector<std::vector<std::string>> rows;
        for (std::size_t i = 0; i < package.programs.size() && i < package.cnc.size(); ++i) {
            const cam::SheetProgram& p = package.cnc[i];
            const int count = i < plan->layouts.size() ? plan->layouts[i].count : 1;
            rows.push_back({relative(package, package.programs[i]), "× " + std::to_string(count),
                            durationText(p.stats.seconds),
                            txt(TextId::HolesLabel) + ": " + std::to_string(p.stats.holes),
                            txt(TextId::ToolChangesLabel) + ": "
                                + std::to_string(p.stats.toolChanges)});
        }
        out += textTable(rows, {false, true, true, false, false});
        out += txt(TextId::TotalTime) + ": "
             + durationText(totalMachiningSeconds(*plan, package.cnc)) + "\n";
        if (const std::string left = leftoverText(package.cnc); !left.empty()) {
            out += heading(txt(TextId::LeftoverTitle)) + left + "\n";
        }
    }

    // --- Parts the workshop does not cut ----------------------------------------------------
    if (outOfPlan > 0) {
        out += heading(txt(TextId::OutOfPlanTitle));
        std::vector<std::vector<std::string>> rows;
        for (std::size_t i = 0; i < cutList.size(); ++i) {
            const detailing::CutListRow& row = cutList[i];
            if (!row.inCuttingPlan) {
                rows.push_back({std::to_string(i + 1), exporting::partName(row), row.material.name,
                                sizeText(row.finishLength, row.finishWidth),
                                "× " + std::to_string(row.quantity * order.quantity)});
            }
        }
        out += textTable(rows, {true, false, false, true, true});
    }

    // --- Files ------------------------------------------------------------------------------
    out += heading(txt(TextId::FilesTitle));
    for (const std::filesystem::path* file :
         {&package.documents, &package.cutListCsv, &package.project}) {
        if (!file->empty()) {
            out += relative(package, *file) + "\n";
        }
    }
    std::vector<std::vector<std::string>> folders;
    const auto folder = [&](const std::vector<std::filesystem::path>& list, const char* name) {
        if (!list.empty()) {
            folders.push_back({std::string(name) + "/",
                               std::to_string(list.size()) + " " + txt(TextId::FilesWord)});
        }
    };
    folder(package.partDrawings, "DXF");
    folder(package.sheetDrawings, "KESIM");
    folder(package.programs, "CNC");
    out += textTable(folders, {false, true});
    return out;
}

std::string packageMessage(const Order& order, const nesting::NestingResult* plan,
                           const Package& package)
{
    std::vector<std::string> parts;
    if (!package.documents.empty()) {
        parts.push_back("1 PDF");
    }
    if (!package.partDrawings.empty()) {
        parts.push_back(std::to_string(package.partDrawings.size()) + " "
                        + txt(TextId::PartDrawingsWord));
    }
    if (!package.sheetDrawings.empty()) {
        parts.push_back(std::to_string(package.sheetDrawings.size()) + " "
                        + txt(TextId::SheetDrawingsWord));
    }
    if (!package.programs.empty()) {
        parts.push_back(std::to_string(package.programs.size()) + " "
                        + txt(TextId::ProgramsWord));
    }
    if (!package.cutListCsv.empty()) {
        parts.push_back(txt(TextId::CutListCsvWord));
    }
    std::string message;
    for (const std::string& p : parts) {
        message += (message.empty() ? "" : " · ") + p;
    }
    if (plan == nullptr) {
        message += "\n" + txt(TextId::NoCuttingPlan);
    } else if (!plan->unplaced.empty()) {
        int count = 0;
        for (const auto& u : plan->unplaced) {
            count += u.quantity;
        }
        message += "\n" + txt(TextId::Unplaced) + ": " + std::to_string(count);
    }
    if (const std::string left = leftoverText(package.cnc); !left.empty()) {
        message += "\n\n" + txt(TextId::LeftoverTitle) + ":\n" + left;
    }
    if (const auto attention = attentionLines(order.document); !attention.empty()) {
        message += "\n\n" + txt(TextId::SummaryAttention) + ":";
        for (const std::string& line : attention) {
            message += "\n" + line;
        }
    }
    return message;
}

} // namespace rc::production
