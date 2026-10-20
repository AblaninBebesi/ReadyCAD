// Stage 7 windows: the cutting plan ("Kesim planı"), the stock of sheets ("Levha stoku") and
// the cutting settings. The optimisation runs on a background thread (nesting::NestingRun);
// these windows show its progress and the best layout so far, live.

#include "EditorPanels.hpp"
#include "FileDialogs.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/SheetDrawing.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Utf8.hpp"
#include "readycad/storage/Storage.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace rc::ui {

using namespace i18n; // tr(), TextId, mm()

namespace ed = rc::editor;
using core::Real;

namespace {

// ---------------------------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------------------------

std::string sizeText(Real a, Real b)
{
    return mm(a) + " × " + mm(b);
}

std::string percent(Real fraction)
{
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%%%.0f", fraction * 100.0);
    return buffer;
}

const char* machineText(nesting::Machine machine)
{
    return tr(machine == nesting::Machine::PanelSaw ? TextId::MachinePanelSaw
                                                    : TextId::MachineNestingCenter);
}

const char* sourceText(nesting::SheetSource source)
{
    switch (source) {
    case nesting::SheetSource::Remnant:  return tr(TextId::FromRemnant);
    case nesting::SheetSource::Stock:    return tr(TextId::FromStock);
    case nesting::SheetSource::Purchase: return tr(TextId::ToPurchase);
    }
    return "";
}

/// Length in mm, dragged or typed; clamped to [min, max].
bool dragMm(const char* id, Real& value, Real min, Real max, const char* format = "%.1f mm")
{
    return ImGui::DragScalar(id, ImGuiDataType_Double, &value, 0.5f, &min, &max, format,
                             ImGuiSliderFlags_AlwaysClamp);
}

float buttonWidth(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

float checkboxWidth(const char* label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x
         + ImGui::CalcTextSize(label, nullptr, true).x;
}

/// A toolbar that flows: the next item of `width` stays on this line while it fits the window,
/// else it starts the next line — nothing is cut off at the right edge.
void nextOnLine(float width)
{
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < width) {
        ImGui::NewLine();
    }
}

/// "Label | widget" row of a two-column settings table.
void row(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool beginSettingsTable(const char* id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) {
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    return true;
}

void saveStockFile(UiState& state)
{
    ++state.stockRevision;
    if (state.stockFile.empty()) {
        return;
    }
    run(state, [&] { storage::saveStockFile(state.stock, state.stockFile); });
}

void saveSettingsFile(UiState& state)
{
    if (state.settingsFile.empty()) {
        return;
    }
    run(state, [&] {
        storage::saveSettingsFile({state.nestingSettings, state.cnc}, state.settingsFile);
    });
}

/// Materials the stock window offers: those of the product, then those already on stock.
std::vector<core::Material> knownMaterials(const UiState& state)
{
    std::vector<core::Material> out;
    const auto addUnique = [&](const core::Material& m) {
        if (std::none_of(out.begin(), out.end(),
                         [&](const core::Material& o) { return nesting::sameMaterial(o, m); })) {
            out.push_back(m);
        }
    };
    for (const detailing::CutListRow& row : state.document->cutList()) {
        addUnique(row.material);
    }
    for (const nesting::Sheet& sheet : state.stock.sheets()) {
        addUnique(sheet.material);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// The sheet picture
// ---------------------------------------------------------------------------------------------

constexpr ImU32 kWaste = IM_COL32(104, 90, 74, 255);    // raw chipboard: what is left over
constexpr ImU32 kPart = IM_COL32(226, 219, 203, 255);   // melamine
constexpr ImU32 kPartHover = IM_COL32(250, 236, 170, 255);
constexpr ImU32 kOutline = IM_COL32(40, 40, 44, 255);
constexpr ImU32 kRemnantFill = IM_COL32(70, 170, 100, 90);
constexpr ImU32 kRemnantLine = IM_COL32(110, 220, 140, 255);
constexpr ImU32 kCut = IM_COL32(235, 70, 60, 220);
constexpr ImU32 kUsable = IM_COL32(255, 255, 255, 70);
constexpr ImU32 kLabel = IM_COL32(25, 25, 28, 255);
constexpr ImU32 kRapid = IM_COL32(120, 200, 255, 110);
constexpr ImU32 kContourPath = IM_COL32(255, 140, 30, 230);
constexpr ImU32 kRouterPath = IM_COL32(40, 210, 220, 230);
constexpr ImU32 kDrill = IM_COL32(200, 40, 160, 255);

bool insidePolygon(ImVec2 p, const std::vector<ImVec2>& poly)
{
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].y > p.y) != (poly[j].y > p.y)
            && p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y)
                         + poly[i].x) {
            in = !in;
        }
    }
    return in;
}

/// Text centred in a screen rectangle if it fits, at the given size.
void centredText(ImDrawList* draw, ImVec2 min, ImVec2 max, const std::string& text, float size,
                 ImU32 colour)
{
    ImFont* font = ImGui::GetFont();
    const float scale = size / ImGui::GetFontSize();
    const ImVec2 t = ImGui::CalcTextSize(text.c_str());
    const ImVec2 extent(t.x * scale, t.y * scale);
    if (extent.x > max.x - min.x - 4.0f || extent.y > max.y - min.y - 2.0f) {
        return;
    }
    draw->AddText(font, size,
                  ImVec2((min.x + max.x - extent.x) / 2.0f, (min.y + max.y - extent.y) / 2.0f),
                  colour, text.c_str());
}

void drawSheet(UiState& state, const nesting::NestingResult& result, std::size_t index,
               const cam::SheetProgram* toolpath)
{
    const nesting::SheetLayout& layout = result.layouts[index];
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 40.0f || avail.y < 40.0f) {
        return;
    }
    ImGui::InvisibleButton("sheet", avail);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const float pad = 12.0f;
    const auto length = static_cast<float>(layout.sheet.length);
    const auto width = static_cast<float>(layout.sheet.width);
    const float scale = std::min((avail.x - 2.0f * pad) / length, (avail.y - 2.0f * pad) / width);
    const ImVec2 corner(origin.x + (avail.x - length * scale) / 2.0f,
                        origin.y + (avail.y - width * scale) / 2.0f);
    // Sheet coordinates (y up) -> screen (y down).
    const auto screen = [&](core::Vec2 p) {
        return ImVec2(corner.x + static_cast<float>(p.x) * scale,
                      corner.y + (width - static_cast<float>(p.y)) * scale);
    };
    const auto box = [&](const nesting::Rect& r) {
        const ImVec2 a = screen(r.min);
        const ImVec2 b = screen(r.max);
        return std::pair(ImVec2(std::min(a.x, b.x), std::min(a.y, b.y)),
                         ImVec2(std::max(a.x, b.x), std::max(a.y, b.y)));
    };

    draw->AddRectFilled(screen({0.0, layout.sheet.width}), screen({layout.sheet.length, 0.0}), kWaste);
    const auto [usableMin, usableMax] = box(layout.usable);
    draw->AddRect(usableMin, usableMax, kUsable);

    for (const nesting::Rect& r : layout.remnants) {
        const auto [a, b] = box(r);
        draw->AddRectFilled(a, b, kRemnantFill);
        draw->AddRect(a, b, kRemnantLine, 0.0f, 1.5f); // ImGui 1.92.8: thickness before flags
        centredText(draw, a, b, std::string(tr(TextId::Remnant)) + " " + sizeText(r.width(), r.height()),
                    ImGui::GetFontSize() * 0.85f, kRemnantLine);
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const nesting::PlacedPart* hover = nullptr;
    for (const nesting::PlacedPart& placed : layout.parts) {
        const nesting::NestingPart& part = result.parts[placed.part];
        const nesting::Polygon outline = nesting::transformed(part.outline, placed.transform);
        std::vector<ImVec2> points;
        for (const core::Vec2& p : outline) {
            points.push_back(screen(p));
        }
        const bool over = hovered && insidePolygon(mouse, points);
        if (over) {
            hover = &placed;
        }
        const nesting::Rect bounds = nesting::bounds(outline);
        const auto [a, b] = box(bounds);
        if (nesting::isRectangle(outline)) {
            draw->AddRectFilled(a, b, over ? kPartHover : kPart);
        } else {
            draw->AddConcavePolyFilled(points.data(), static_cast<int>(points.size()),
                                       over ? kPartHover : kPart);
        }
        draw->AddPolyline(points.data(), static_cast<int>(points.size()), kOutline, 1.0f,
                          ImDrawFlags_Closed);
        const auto [la, lb] = box(nesting::labelArea(outline));
        centredText(draw, la, lb, part.label, ImGui::GetFontSize(), kLabel);
    }

    if (state.showSawCuts) {
        for (const nesting::SawCut& cut : layout.cuts) {
            draw->AddLine(screen(cut.from), screen(cut.to), kCut, cut.stage == 0 ? 1.0f : 1.5f);
        }
    }

    // The CNC toolpath seen from above: travel thin, cuts in the colour of their tool, holes as
    // rings. Routers: orange (contours), cyan (grooves, milled holes).
    if (toolpath != nullptr) {
        std::optional<core::Vec3> at;
        int tool = 0;
        for (const cam::Step& step : toolpath->program.steps) {
            if (const auto* t = std::get_if<cam::ToolChange>(&step)) {
                tool = t->tool.number;
            } else if (const auto* r = std::get_if<cam::Rapid>(&step)) {
                if (at && (at->x != r->to.x || at->y != r->to.y)) {
                    draw->AddLine(screen({at->x, at->y}), screen({r->to.x, r->to.y}), kRapid, 1.0f);
                }
                at = r->to;
            } else if (const auto* f = std::get_if<cam::Feed>(&step)) {
                if (at) {
                    const bool contour = tool == state.cnc.contourTool;
                    draw->AddLine(screen({at->x, at->y}), screen({f->to.x, f->to.y}),
                                  contour ? kContourPath : kRouterPath, contour ? 2.0f : 1.5f);
                }
                at = f->to;
            } else if (const auto* d = std::get_if<cam::DrillCycle>(&step)) {
                if (at && (at->x != d->at.x || at->y != d->at.y)) {
                    draw->AddLine(screen({at->x, at->y}), screen(d->at), kRapid, 1.0f);
                }
                draw->AddCircle(screen(d->at), 3.0f, kDrill, 12, 1.5f);
                at = core::Vec3(d->at.x, d->at.y, d->clearance);
            }
        }
    }

    if (hover != nullptr) {
        const nesting::NestingPart& part = result.parts[hover->part];
        const nesting::Rect blank = nesting::bounds(part.outline);
        std::string name = part.label;
        if (part.cutListRow && *part.cutListRow < state.document->cutList().size()) {
            name += ". " + exporting::partName(state.document->cutList()[*part.cutListRow]);
        }
        ImGui::SetTooltip("%s\n%s: %s mm\n%.0f°", name.c_str(), tr(TextId::CutSize),
                          sizeText(blank.width(), blank.height()).c_str(),
                          hover->transform.degrees());
    }
}

// ---------------------------------------------------------------------------------------------
// The list of sheets
// ---------------------------------------------------------------------------------------------

void drawSheetList(UiState& state, const nesting::NestingResult& result, bool fresh)
{
    std::string material;
    for (std::size_t i = 0; i < result.layouts.size(); ++i) {
        const nesting::SheetLayout& l = result.layouts[i];
        if (l.sheet.material.name != material) {
            material = l.sheet.material.name;
            ImGui::SeparatorText(material.c_str());
        }
        char line[160];
        std::snprintf(line, sizeof line, "%zu.  %s  ×%d   %s   %s##sheet%zu", i + 1,
                      sizeText(l.sheet.length, l.sheet.width).c_str(), l.count,
                      percent(l.utilization()).c_str(), sourceText(l.source), i);
        if (ImGui::Selectable(line, state.selectedSheet == i)) {
            state.selectedSheet = i;
        }
    }

    ImGui::Separator();
    int stock = 0;
    int remnants = 0;
    int purchase = 0;
    int produced = 0;
    for (const nesting::MaterialUsage& u : result.usage()) {
        stock += u.stock;
        remnants += u.remnants;
        purchase += u.purchase;
        produced += u.newRemnants;
    }
    ImGui::Text("%s: %d   %s: %s", tr(TextId::SheetsTotal), result.sheetCount(),
                tr(TextId::Utilization), percent(result.utilization()).c_str());
    ImGui::Text("%s: %d   %s: %d", tr(TextId::FromStock), stock, tr(TextId::FromRemnant), remnants);
    ImGui::Text("%s: %d   %s: %d", tr(TextId::ToPurchase), purchase, tr(TextId::NewRemnants),
                produced);
    if (!result.settings.useRemnants) {
        ImGui::TextDisabled("%s", tr(TextId::RemnantsNotUsed));
    }
    if (result.settings.machine == nesting::Machine::PanelSaw && !result.layouts.empty()) {
        int stacks = 0;
        for (const nesting::SheetLayout& l : result.layouts) {
            const int per = nesting::sheetsPerStack(result.settings.saw, l.sheet.material.thickness);
            stacks += (l.count + per - 1) / per;
        }
        ImGui::Text("%s: %d", tr(TextId::StackCount), stacks);
    }
    if (!result.unplaced.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s:", tr(TextId::Unplaced));
        for (const nesting::NestingResult::Unplaced& u : result.unplaced) {
            const nesting::NestingPart& p = result.parts[u.part];
            const nesting::Rect b = nesting::bounds(p.outline);
            ImGui::BulletText("%s  %s  ×%d", p.label.c_str(), sizeText(b.width(), b.height()).c_str(),
                              u.quantity);
        }
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!fresh || state.nestingConsumed || result.layouts.empty());
    if (ImGui::Button(tr(TextId::MenuConsumeStock))) {
        consumeStock(state);
    }
    ImGui::EndDisabled();
    if (!state.nestingMessage.empty()) {
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s", state.nestingMessage.c_str());
    }
}

// ---------------------------------------------------------------------------------------------
// Popups
// ---------------------------------------------------------------------------------------------

void drawSettingsPopup(UiState& state)
{
    if (state.openNestingSettings) {
        state.settingsDraft = state.nestingSettings;
        ImGui::OpenPopup(tr(TextId::NestingSettingsWindow));
        state.openNestingSettings = false;
    }
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 30.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(tr(TextId::NestingSettingsWindow), nullptr)) {
        return;
    }
    nesting::NestingSettings& s = state.settingsDraft;

    ImGui::SeparatorText(tr(TextId::SawSettings));
    if (beginSettingsTable("saw")) {
        row(tr(TextId::Kerf));
        dragMm("##kerf", s.saw.kerf, 0.0, 10.0);
        row(tr(TextId::Trim));
        dragMm("##trim", s.saw.trim, 0.0, 50.0);
        row(tr(TextId::Stages));
        ImGui::SliderInt("##stages", &s.saw.maxStages, 2, 4);
        row(tr(TextId::StackHeight));
        dragMm("##stack", s.saw.maxStackHeight, 1.0, 200.0, "%.0f mm");
        row(tr(TextId::FirstCuts));
        const std::pair<nesting::CutDirection, TextId> directions[] = {
            {nesting::CutDirection::AlongLength, TextId::AlongLength},
            {nesting::CutDirection::AlongWidth, TextId::AlongWidth},
            {nesting::CutDirection::Auto, TextId::AutoDirection}};
        const char* current = "";
        for (const auto& [d, t] : directions) {
            if (d == s.saw.firstCuts) {
                current = tr(t);
            }
        }
        if (ImGui::BeginCombo("##firstcuts", current)) {
            for (const auto& [d, t] : directions) {
                if (ImGui::Selectable(tr(t), d == s.saw.firstCuts)) {
                    s.saw.firstCuts = d;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText(tr(TextId::RouterSettings));
    if (beginSettingsTable("router")) {
        row(tr(TextId::ToolDiameter));
        dragMm("##tool", s.router.toolDiameter, 1.0, 40.0);
        row(tr(TextId::ExtraSpacing));
        dragMm("##spacing", s.router.extraSpacing, 0.0, 20.0);
        row(tr(TextId::SheetMargin));
        dragMm("##margin", s.router.margin, 0.0, 50.0);
        ImGui::EndTable();
    }

    ImGui::SeparatorText(tr(TextId::GaSettings));
    if (beginSettingsTable("ga")) {
        row(tr(TextId::Population));
        ImGui::SliderInt("##population", &s.optimizer.populationSize, 2, 200);
        row(tr(TextId::Generations));
        ImGui::SliderInt("##generations", &s.optimizer.generations, 0, 500);
        row(tr(TextId::MutationRate));
        float rate = static_cast<float>(s.optimizer.mutationRate * 100.0);
        if (ImGui::SliderFloat("##mutation", &rate, 0.0f, 100.0f, "%%%.0f")) {
            s.optimizer.mutationRate = static_cast<Real>(rate) / 100.0;
        }
        row(tr(TextId::Elites));
        ImGui::SliderInt("##elites", &s.optimizer.elites, 0, 10);
        row(tr(TextId::Seed));
        ImGui::InputScalar("##seed", ImGuiDataType_U64, &s.optimizer.seed);
        ImGui::EndTable();
    }

    ImGui::SeparatorText(tr(TextId::ComputeResources));
    if (beginSettingsTable("resources")) {
        row(tr(TextId::Threads));
        char threads[48];
        std::snprintf(threads, sizeof threads, "%%d  (%s → %d)", tr(TextId::ThreadsHint),
                      nesting::effectiveThreads(0));
        ImGui::SliderInt("##threads", &s.optimizer.threads, 0, 64, threads);
        row(tr(TextId::MemoryLimit));
        ImGui::SliderInt("##memory", &s.optimizer.memoryLimitMb, 0, 4096, "%d MB");
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button(tr(TextId::Save))) {
        if (run(state, [&] { nesting::validate(s); })) {
            state.nestingSettings = s;
            saveSettingsFile(state);
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::Defaults))) {
        const nesting::Machine machine = s.machine;
        const bool useRemnants = s.useRemnants;
        s = {};
        s.machine = machine;
        s.useRemnants = useRemnants;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::Cancel))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawConsumePopup(UiState& state)
{
    if (state.openConsumePopup) {
        ImGui::OpenPopup(tr(TextId::ConsumeTitle));
        state.openConsumePopup = false;
    }
    if (!ImGui::BeginPopupModal(tr(TextId::ConsumeTitle), nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextUnformatted(tr(TextId::ConsumeBody));
    ImGui::Spacing();
    if (ImGui::Button(tr(TextId::MenuConsumeStock))) {
        if (nestingIsFresh(state) && state.nesting && !state.nestingConsumed) {
            const std::string note =
                production::orderName(*state.document) + " · " + exporting::todayText();
            if (run(state, [&] { state.stock.consume(*state.nesting, note); })) {
                saveStockFile(state);
                state.nestingFor.stock = state.stockRevision; // the plan was made for this stock
                state.nestingConsumed = true;
                state.nestingMessage = tr(TextId::ConsumeDone);
            }
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::Cancel))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------

void loadWorkshopFiles(UiState& state, const std::filesystem::path& folder)
{
    const storage::WorkshopFiles files = storage::WorkshopFiles::in(folder);
    state.stockFile = files.stock;
    state.settingsFile = files.settings;
    // Each file on its own: a broken settings file must not cost the stock, and the other way.
    run(state, [&] { state.stock = storage::loadStockFile(files.stock); });
    run(state, [&] {
        const storage::AppSettings settings = storage::loadSettingsFile(files.settings);
        state.nestingSettings = settings.nesting;
        state.cnc = settings.cnc;
    });
    ++state.stockRevision;
}

bool nestingIsFresh(const UiState& state)
{
    return state.nesting && !state.nestingRun && state.document
        && state.nestingFor.document == state.documentSerial
        && state.nestingFor.revision == state.document->revision()
        && state.nestingFor.stock == state.stockRevision
        && state.nestingFor.productCount == state.productCount
        && state.nesting->settings == state.nestingSettings;
}

void startNesting(UiState& state)
{
    if (state.nestingRun) {
        return;
    }
    const ed::Document& doc = *state.document;
    run(state, [&] {
        nesting::validate(state.nestingSettings);
        std::vector<nesting::NestingPart> parts =
            production::cuttingParts({doc, state.productCount});
        state.nestingRun = std::make_unique<nesting::NestingRun>(std::move(parts), state.stock,
                                                                 state.nestingSettings);
        state.nestingVersion = 0;
        state.nesting.reset();
        ++state.nestingChanges;
        state.nestingFor = {state.documentSerial, doc.revision(), state.stockRevision,
                            state.productCount};
        state.nestingConsumed = false;
        state.nestingMessage.clear();
        state.selectedSheet = 0;
        state.focusNesting = 3;
    });
}

void stopNesting(UiState& state)
{
    if (state.nestingRun) {
        state.nestingRun->cancel();
    }
    state.pendingPackage.reset(); // stopped by hand: no package from a plan cut short
}

void updateNesting(UiState& state)
{
    if (!state.nestingRun) {
        return;
    }
    nesting::NestingRun& run = *state.nestingRun;
    const bool finished = run.finished(); // before reading: a final result is never missed
    if (run.version() != state.nestingVersion) {
        state.nestingVersion = run.version();
        state.nesting = run.result();
        ++state.nestingChanges;
    }
    if (finished) {
        if (!run.error().empty()) {
            state.errorMessage = run.error();
            state.openErrorPopup = true;
        }
        state.nestingRun.reset(); // joins the finished thread
        // A production package was asked for before the plan existed.
        if (state.pendingPackage) {
            const std::filesystem::path parent = *state.pendingPackage;
            state.pendingPackage.reset();
            if (nestingIsFresh(state) && writePackageInto(state, parent)) {
                state.openExportedPopup = true;
            }
        }
    }
    if (state.nesting && state.selectedSheet >= state.nesting->layouts.size()) {
        state.selectedSheet = 0;
    }
}

void consumeStock(UiState& state)
{
    if (nestingIsFresh(state) && !state.nestingConsumed) {
        state.openConsumePopup = true;
    }
}

void exportSheetsDxf(UiState& state)
{
    if (!nestingIsFresh(state) || state.nesting->layouts.empty()) {
        state.errorMessage = tr(TextId::NestingEmpty);
        state.openErrorPopup = true;
        return;
    }
    const auto folder = showFolderDialog();
    if (!folder) {
        return;
    }
    const ed::Document& doc = *state.document;
    const std::filesystem::path target =
        *folder / (safeFileName(production::orderName(doc)) + "_KESIM");
    std::size_t count = 0;
    if (run(state, [&] {
            count = production::writeSheetDxfs({doc, state.productCount}, *state.nesting, target)
                        .size();
        })) {
        state.exportMessage = std::string(tr(TextId::ExportDoneFiles)) + " " + std::to_string(count);
        state.exportedFile = target;
        state.openExportedPopup = true;
    }
}

// ---------------------------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------------------------

void drawNestingWindow(UiState& state)
{
    if (state.focusNesting > 0) {
        ImGui::SetNextWindowFocus();
        --state.focusNesting;
    }
    if (!ImGui::Begin(tr(TextId::WindowNesting))) {
        ImGui::End();
        return;
    }
    const bool running = state.nestingRun != nullptr;

    // Toolbar: machine, number of products, start/stop, settings.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(TextId::Machine));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 13.0f);
    ImGui::BeginDisabled(running);
    if (ImGui::BeginCombo("##machine", machineText(state.nestingSettings.machine))) {
        for (const nesting::Machine m :
             {nesting::Machine::PanelSaw, nesting::Machine::NestingCenter}) {
            if (ImGui::Selectable(machineText(m), m == state.nestingSettings.machine)
                && m != state.nestingSettings.machine) {
                state.nestingSettings.machine = m;
                saveSettingsFile(state);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(tr(TextId::ProductCount));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
    if (ImGui::InputInt("##count", &state.productCount)) {
        state.productCount = std::clamp(state.productCount, 1, 100);
    }
    nextOnLine(checkboxWidth(tr(TextId::UseRemnants)));
    if (ImGui::Checkbox(tr(TextId::UseRemnants), &state.nestingSettings.useRemnants)) {
        saveSettingsFile(state);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        ImGui::SetTooltip("%s", tr(TextId::UseRemnantsHint));
    }
    ImGui::EndDisabled();
    const char* startStop = tr(running ? TextId::Stop : TextId::Calculate);
    nextOnLine(buttonWidth(startStop));
    if (ImGui::Button(startStop)) {
        if (running) {
            stopNesting(state);
        } else {
            startNesting(state);
        }
    }
    nextOnLine(buttonWidth(tr(TextId::SettingsButton)));
    ImGui::BeginDisabled(running);
    if (ImGui::Button(tr(TextId::SettingsButton))) {
        state.openNestingSettings = true;
    }
    ImGui::EndDisabled();
    const bool saw = !state.nesting || state.nesting->settings.machine == nesting::Machine::PanelSaw;
    const char* show = tr(saw ? TextId::ShowCuts : TextId::ShowToolpath);
    nextOnLine(checkboxWidth(show));
    ImGui::Checkbox(show, saw ? &state.showSawCuts : &state.showToolpath);

    if (running) {
        const nesting::NestingProgress p = state.nestingRun->progress();
        const float perMaterial =
            p.generations > 0 ? static_cast<float>(p.generation) / static_cast<float>(p.generations)
                              : 1.0f;
        const float fraction =
            p.materials > 0 ? (static_cast<float>(p.material) + perMaterial)
                                  / static_cast<float>(p.materials)
                            : 0.0f;
        char overlay[160];
        std::snprintf(overlay, sizeof overlay, "%s  %s %d / %d   %s %s   %s %.1f MB",
                      tr(TextId::Computing), tr(TextId::Generation), p.generation, p.generations,
                      tr(TextId::Utilization), percent(p.utilization).c_str(),
                      tr(TextId::CacheMemory), static_cast<double>(p.cacheBytes) / (1024.0 * 1024.0));
        ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.0f), overlay);
    } else if (state.nesting && !nestingIsFresh(state)) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s", tr(TextId::NestingStale));
    }
    if (state.pendingPackage) {
        ImGui::TextDisabled("%s", tr(TextId::PackageWaiting));
    }

    if (!state.nesting) {
        if (!running) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", tr(TextId::NestingEmpty));
            if (state.stock.sheets().empty()) {
                ImGui::TextDisabled("%s", tr(TextId::StockEmpty));
            }
        }
        ImGui::End();
        return;
    }

    const nesting::NestingResult& result = *state.nesting;
    const float listWidth = ImGui::GetFontSize() * 19.0f;
    if (ImGui::BeginChild("sheets", ImVec2(listWidth, 0.0f), ImGuiChildFlags_Borders)) {
        drawSheetList(state, result, nestingIsFresh(state));
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("picture", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
        if (state.selectedSheet < result.layouts.size()) {
            const nesting::SheetLayout& l = result.layouts[state.selectedSheet];
            ImGui::Text("%s  ·  %s  ·  %s  ·  %s %s", l.sheet.material.name.c_str(),
                        sizeText(l.sheet.length, l.sheet.width).c_str(), sourceText(l.source),
                        tr(TextId::Utilization), percent(l.utilization()).c_str());
            const cam::SheetProgram* toolpath = nullptr;
            if (state.showToolpath && result.settings.machine == nesting::Machine::NestingCenter) {
                toolpath = sheetToolpath(state);
                if (toolpath != nullptr) {
                    ImGui::TextWrapped("%s", toolpathSummary(*toolpath).c_str());
                } else if (!state.toolpathError.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s",
                                       state.toolpathError.c_str());
                }
            }
            drawSheet(state, result, state.selectedSheet, toolpath);
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void drawStockWindow(UiState& state)
{
    if (state.focusStock > 0) {
        ImGui::SetNextWindowFocus();
        --state.focusStock;
    }
    if (!ImGui::Begin(tr(TextId::WindowStock))) {
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("%s: %s", tr(TextId::StockFile), toUtf8(state.stockFile).c_str());

    bool changed = false;
    std::optional<nesting::SheetId> removed;
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY
                                    | ImGuiTableFlags_SizingStretchProp;
    const float tableHeight = std::max(ImGui::GetFontSize() * 6.0f,
                                       ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 6.5f);
    if (ImGui::BeginTable("stock", 6, flags, ImVec2(0.0f, tableHeight))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const TextId column : {TextId::ColMaterial, TextId::ColSheetSize, TextId::ColQuantity,
                                    TextId::ColKind, TextId::ColNote}) {
            ImGui::TableSetupColumn(tr(column));
        }
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2.0f);
        ImGui::TableHeadersRow();
        for (const nesting::Sheet& sheet : state.stock.sheets()) {
            ImGui::PushID(static_cast<int>(sheet.id));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(sheet.material.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(sizeText(sheet.length, sheet.width).c_str());
            ImGui::TableNextColumn();
            int quantity = sheet.quantity;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##q", &quantity, 1, 5) && quantity >= 0) {
                nesting::Sheet updated = sheet;
                updated.quantity = quantity;
                run(state, [&] { state.stock.update(updated); });
                changed = true;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr(sheet.remnant ? TextId::Remnant : TextId::SheetFull));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(sheet.note.c_str());
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X")) {
                removed = sheet.id;
            }
            ImGui::PopID();
            if (changed) {
                break; // the list changed under the loop
            }
        }
        ImGui::EndTable();
    }
    if (removed) {
        state.stock.remove(*removed);
        changed = true;
    }

    // Add sheets of a material of the product or one already on stock.
    ImGui::SeparatorText(tr(TextId::AddSheet));
    const std::vector<core::Material> materials = knownMaterials(state);
    state.newSheetMaterial =
        std::clamp(state.newSheetMaterial, 0, std::max(0, static_cast<int>(materials.size()) - 1));
    if (!materials.empty()) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        if (ImGui::BeginCombo("##material", materials[static_cast<std::size_t>(state.newSheetMaterial)].name.c_str())) {
            for (std::size_t i = 0; i < materials.size(); ++i) {
                if (ImGui::Selectable(materials[i].name.c_str(),
                                      static_cast<int>(i) == state.newSheetMaterial)) {
                    state.newSheetMaterial = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.5f);
        dragMm("##length", state.newSheet.length, 50.0, 6000.0, "%.0f");
        ImGui::SameLine();
        ImGui::TextUnformatted("×");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.5f);
        dragMm("##width", state.newSheet.width, 50.0, 3000.0, "%.0f mm");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
        ImGui::InputInt("##qty", &state.newSheet.quantity, 1, 5);
        state.newSheet.quantity = std::max(1, state.newSheet.quantity);
        ImGui::SameLine();
        ImGui::Checkbox(tr(TextId::Remnant), &state.newSheet.remnant);
        ImGui::SameLine();
        if (ImGui::Button(tr(TextId::AddSheet))) {
            nesting::Sheet sheet = state.newSheet;
            sheet.material = materials[static_cast<std::size_t>(state.newSheetMaterial)];
            if (run(state, [&] { state.stock.add(sheet); })) {
                changed = true;
            }
        }
    }

    // Workshop rules of the stock.
    ImGui::SeparatorText(tr(TextId::Construction));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(TextId::StandardSheet));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.5f);
    dragMm("##stdl", state.stock.standardLength, 100.0, 6000.0, "%.0f");
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SameLine();
    ImGui::TextUnformatted("×");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.5f);
    dragMm("##stdw", state.stock.standardWidth, 100.0, 3000.0, "%.0f mm");
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(TextId::MinRemnant));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.5f);
    dragMm("##minl", state.stock.remnantRules.minLength, 0.0, 3000.0, "%.0f");
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SameLine();
    ImGui::TextUnformatted("×");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.5f);
    dragMm("##minw", state.stock.remnantRules.minWidth, 0.0, 3000.0, "%.0f mm");
    changed |= ImGui::IsItemDeactivatedAfterEdit();

    if (changed) {
        saveStockFile(state);
    }
    ImGui::End();
}

void drawNestingPopups(UiState& state)
{
    drawSettingsPopup(state);
    drawConsumePopup(state);
}

} // namespace rc::ui
