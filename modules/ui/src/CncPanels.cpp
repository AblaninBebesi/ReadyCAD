// Stage 8: CNC programs for the nesting centre — export, the toolpath of a sheet for the preview
// in the cutting plan window, and the settings of the machine (tools, controller dialect).

#include "EditorPanels.hpp"
#include "FileDialogs.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Utf8.hpp"
#include "readycad/production/Summary.hpp"
#include "readycad/storage/Storage.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

namespace rc::ui {

using namespace i18n; // tr(), TextId, mm()
using core::Real;

namespace {

bool cncResultReady(const UiState& state)
{
    return nestingIsFresh(state) && !state.nesting->layouts.empty()
        && state.nesting->settings.machine == nesting::Machine::NestingCenter;
}

bool dragMm(const char* id, Real& value, Real min, Real max, const char* format = "%.1f mm")
{
    return ImGui::DragScalar(id, ImGuiDataType_Double, &value, 0.1f, &min, &max, format,
                             ImGuiSliderFlags_AlwaysClamp);
}

void row(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

void drawToolTable(std::vector<cam::Tool>& tools)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("tools", 9, flags)) {
        return;
    }
    ImGui::TableSetupColumn("T", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    const std::pair<TextId, float> columns[] = {
        {TextId::ToolKindLabel, 1.3f}, {TextId::ColDiameter, 0.7f}, {TextId::CuttingLength, 0.8f},
        {TextId::Rpm, 0.9f},           {TextId::FeedRate, 0.9f},   {TextId::PlungeFeed, 0.8f},
        {TextId::StepDown, 0.7f}};
    for (const auto& [column, weight] : columns) {
        ImGui::TableSetupColumn(tr(column), ImGuiTableColumnFlags_WidthStretch, weight);
    }
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 1.6f);
    ImGui::TableHeadersRow();
    std::optional<std::size_t> removed;
    for (std::size_t i = 0; i < tools.size(); ++i) {
        cam::Tool& t = tools[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        const auto cell = [] {
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
        };
        cell();
        ImGui::InputInt("##n", &t.number, 0, 0);
        cell();
        const char* kind = tr(t.kind == cam::ToolKind::Router ? TextId::ToolRouter : TextId::ToolDrill);
        if (ImGui::BeginCombo("##kind", kind)) {
            if (ImGui::Selectable(tr(TextId::ToolRouter), t.kind == cam::ToolKind::Router)) {
                t.kind = cam::ToolKind::Router;
            }
            if (ImGui::Selectable(tr(TextId::ToolDrill), t.kind == cam::ToolKind::Drill)) {
                t.kind = cam::ToolKind::Drill;
            }
            ImGui::EndCombo();
        }
        cell();
        dragMm("##d", t.diameter, 0.5, 60.0, "%.1f");
        cell();
        dragMm("##l", t.cuttingLength, 1.0, 120.0, "%.0f");
        cell();
        ImGui::DragInt("##rpm", &t.rpm, 100.0f, 500, 30000);
        cell();
        if (t.kind == cam::ToolKind::Router) {
            dragMm("##f", t.feed, 100.0, 60000.0, "%.0f");
        } else {
            ImGui::TextDisabled("—");
        }
        cell();
        dragMm("##p", t.plungeFeed, 100.0, 30000.0, "%.0f");
        cell();
        if (t.kind == cam::ToolKind::Router) {
            dragMm("##s", t.stepDown, 0.5, 60.0, "%.1f");
        } else {
            ImGui::TextDisabled("—");
        }
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
            removed = i;
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
    if (removed) {
        tools.erase(tools.begin() + static_cast<std::ptrdiff_t>(*removed));
    }
    if (ImGui::Button(tr(TextId::AddTool))) {
        int next = 1;
        for (const cam::Tool& t : tools) {
            next = std::max(next, t.number + 1);
        }
        cam::Tool tool;
        tool.number = next;
        tool.kind = cam::ToolKind::Drill;
        tool.diameter = 10.0;
        tool.rpm = 4000;
        tool.plungeFeed = 2000.0;
        tools.push_back(tool);
    }
}

void drawSettingsPopup(UiState& state)
{
    if (state.openCncSettings) {
        state.cncDraft = state.cnc;
        ImGui::OpenPopup(tr(TextId::CncSettingsWindow));
        state.openCncSettings = false;
    }
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 44.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(tr(TextId::CncSettingsWindow), nullptr)) {
        return;
    }
    cam::NestingCenterProfile& p = state.cncDraft;

    ImGui::SeparatorText(tr(TextId::PostprocessorLabel));
    if (ImGui::BeginTable("dialect", 2, ImGuiTableFlags_SizingStretchProp)) {
        row(tr(TextId::Preset));
        if (ImGui::BeginCombo("##preset", p.dialect.name.c_str())) {
            for (const cam::GCodeDialect& preset : cam::dialectPresets()) {
                if (ImGui::Selectable(preset.name.c_str(), preset.name == p.dialect.name)) {
                    p.dialect = preset;
                }
            }
            ImGui::EndCombo();
        }
        row(tr(TextId::ZeroPlaneLabel));
        if (ImGui::RadioButton(tr(TextId::ZeroTable), p.dialect.zero == cam::ZeroPlane::Table)) {
            p.dialect.zero = cam::ZeroPlane::Table;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(tr(TextId::ZeroTop), p.dialect.zero == cam::ZeroPlane::Top)) {
            p.dialect.zero = cam::ZeroPlane::Top;
        }
        row(tr(TextId::Decimals));
        ImGui::SliderInt("##decimals", &p.dialect.decimals, 0, 5);
        row(tr(TextId::LineNumbers));
        ImGui::Checkbox("##lines", &p.dialect.lineNumbers);
        row(tr(TextId::DrillCycles));
        ImGui::Checkbox("##cycles", &p.dialect.drillCycles);
        ImGui::EndTable();
    }

    ImGui::SeparatorText(tr(TextId::MachineNestingCenter));
    if (ImGui::BeginTable("machine", 2, ImGuiTableFlags_SizingStretchProp)) {
        row(tr(TextId::ContourTool));
        const auto toolLabel = [](const cam::Tool& t) {
            return "T" + std::to_string(t.number) + "  D" + mm(t.diameter) + "  " + t.name;
        };
        std::string current = "T" + std::to_string(p.contourTool);
        for (const cam::Tool& t : p.tools) {
            if (t.number == p.contourTool) {
                current = toolLabel(t);
            }
        }
        if (ImGui::BeginCombo("##contour", current.c_str())) {
            for (const cam::Tool& t : p.tools) {
                if (t.kind == cam::ToolKind::Router) {
                    const std::string label = toolLabel(t);
                    if (ImGui::Selectable(label.c_str(), t.number == p.contourTool)) {
                        p.contourTool = t.number;
                    }
                }
            }
            ImGui::EndCombo();
        }
        row(tr(TextId::SafeHeight));
        dragMm("##safe", p.safeHeight, 1.0, 200.0);
        row(tr(TextId::ClearanceLabel));
        dragMm("##clear", p.clearance, 0.5, 50.0);
        row(tr(TextId::SpoilboardCut));
        dragMm("##spoil", p.spoilboardCut, 0.0, 3.0, "%.2f mm");
        row(tr(TextId::ClimbMilling));
        ImGui::Checkbox("##climb", &p.climbMilling);
        ImGui::EndTable();
    }

    ImGui::SeparatorText(tr(TextId::ToolsHeader));
    drawToolTable(p.tools);

    ImGui::Spacing();
    if (ImGui::Button(tr(TextId::Save))) {
        if (run(state, [&] { cam::validate(p); })) {
            state.cnc = p;
            run(state, [&] {
                std::filesystem::create_directories(state.settingsFile.parent_path());
                storage::writeTextFile(state.settingsFile,
                                       storage::saveSettings({state.nestingSettings, state.cnc}));
            });
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::Defaults))) {
        p = cam::defaultProfile();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::Cancel))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace

bool writeCncPrograms(UiState& state, const std::filesystem::path& folder)
{
    if (!cncResultReady(state)) {
        state.errorMessage = tr(TextId::CncNeedsRouter);
        state.openErrorPopup = true;
        return false;
    }
    std::vector<cam::SheetProgram> programs;
    std::size_t count = 0;
    if (!run(state, [&] {
            count = production::writeCncPrograms({*state.document, state.productCount},
                                                 *state.nesting, state.cnc, folder, &programs)
                        .size();
        })) {
        return false;
    }
    std::string message = std::string(tr(TextId::CncProgramsDone)) + " " + std::to_string(count);
    if (const std::string left = production::leftoverText(programs); !left.empty()) {
        message += "\n\n" + std::string(tr(TextId::LeftoverTitle)) + ":\n" + left;
    }
    state.exportMessage = message;
    state.exportedFile = folder;
    return true;
}

void exportCncPrograms(UiState& state)
{
    if (!cncResultReady(state)) {
        state.errorMessage = tr(TextId::CncNeedsRouter);
        state.openErrorPopup = true;
        return;
    }
    const auto folder = showFolderDialog();
    if (!folder) {
        return;
    }
    const std::filesystem::path target =
        *folder / (safeFileName(production::orderName(*state.document)) + "_CNC");
    if (writeCncPrograms(state, target)) {
        state.openExportedPopup = true;
    }
}

const cam::SheetProgram* sheetToolpath(UiState& state)
{
    if (!state.nesting || state.nesting->settings.machine != nesting::Machine::NestingCenter
        || state.selectedSheet >= state.nesting->layouts.size()) {
        return nullptr;
    }
    const UiState::ToolpathKey key{state.nestingChanges, state.selectedSheet, state.cnc};
    if (!state.toolpathKey || !(*state.toolpathKey == key)) {
        state.toolpathKey = key;
        state.toolpath.reset();
        state.toolpathError.clear();
        try {
            const editor::Document& doc = *state.document;
            const detailing::PartSource source{doc.product(), doc.cutList()};
            state.toolpath = cam::planSheet(cam::sheetJob(*state.nesting, state.selectedSheet, &source),
                                            state.cnc);
        } catch (const std::exception& e) {
            state.toolpathError = e.what();
        }
    }
    return state.toolpath ? &*state.toolpath : nullptr;
}

std::string toolpathSummary(const cam::SheetProgram& program)
{
    std::string text = std::string(tr(TextId::EstimatedTime)) + ": "
                     + production::durationText(program.stats.seconds) + "   "
                     + tr(TextId::HolesLabel) + ": " + std::to_string(program.stats.holes) + "   "
                     + tr(TextId::ToolChangesLabel) + ": "
                     + std::to_string(program.stats.toolChanges);
    if (program.milledHoles > 0) {
        text += "   " + std::string(tr(TextId::MilledHolesLabel)) + ": "
              + std::to_string(program.milledHoles);
    }
    if (const std::string left = production::leftoverText({program}); !left.empty()) {
        std::string oneLine;
        for (const char c : left) {
            oneLine += c == '\n' ? std::string("; ") : std::string(1, c);
        }
        text += "\n" + std::string(tr(TextId::LeftoverTitle)) + ": " + oneLine;
    }
    return text;
}

void drawCncPopups(UiState& state)
{
    drawSettingsPopup(state);
}

} // namespace rc::ui
