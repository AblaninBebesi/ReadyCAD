#include "EditorPanels.hpp"

#include "FileDialogs.hpp"
#include "readycad/core/model/Drawer.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/editor/PartInfo.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Utf8.hpp"
#include "readycad/production/Summary.hpp"
#include "readycad/storage/Storage.hpp"

#include <glm/geometric.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <utility>

namespace rc::ui {

using namespace i18n; // tr(), TextId, displayName(), mm(): the UI is mostly texts

namespace ed = rc::editor;
using core::ComponentKind;
using core::Real;

namespace {

// ---------------------------------------------------------------------------------------------
// Small widgets
// ---------------------------------------------------------------------------------------------

/// Two-column "label | widget" table, closed automatically.
class PropertyTable {
public:
    explicit PropertyTable(const char* id)
        : open_(ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
    {
        if (open_) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.42f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.58f);
        }
    }
    ~PropertyTable()
    {
        if (open_) {
            ImGui::EndTable();
        }
    }
    PropertyTable(const PropertyTable&) = delete;
    PropertyTable& operator=(const PropertyTable&) = delete;

    [[nodiscard]] bool isOpen() const noexcept { return open_; }

    /// Starts a row: the label on the left, the next widget fills the right column.
    static void row(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::PushTextWrapPos(0.0f); // long labels wrap in their column instead of being cut
        ImGui::TextUnformatted(label);
        ImGui::PopTextWrapPos();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    static void textRow(const char* label, const std::string& value)
    {
        row(label);
        ImGui::TextUnformatted(value.c_str());
    }

private:
    bool open_;
};

template <typename E, std::size_t N>
bool enumCombo(const char* id, E& value, const std::pair<E, TextId> (&items)[N])
{
    const char* preview = "?";
    for (const auto& [v, t] : items) {
        if (v == value) {
            preview = tr(t);
        }
    }
    bool changed = false;
    if (ImGui::BeginCombo(id, preview)) {
        for (const auto& [v, t] : items) {
            const bool selected = v == value;
            if (ImGui::Selectable(tr(t), selected) && !selected) {
                value = v;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

/// Length in mm, dragged or typed; clamped to [min, max].
bool dragMm(const char* id, Real& value, Real min, Real max)
{
    const std::string format = "%.0f" + mmUnit();
    return ImGui::DragScalar(id, ImGuiDataType_Double, &value, 1.0f, &min, &max, format.c_str(),
                             ImGuiSliderFlags_AlwaysClamp);
}

constexpr std::pair<core::JointType, TextId> kJoints[] = {
    {core::JointType::Overlay, TextId::JointOverlay},
    {core::JointType::Inset, TextId::JointInset},
};
constexpr std::pair<core::BackMountType, TextId> kBackMounts[] = {
    {core::BackMountType::Groove, TextId::BackGroove},
    {core::BackMountType::Overlay, TextId::BackOverlay},
    {core::BackMountType::Inset, TextId::BackInset},
};
constexpr std::pair<core::ConstructionRules::JointFastener, TextId> kFasteners[] = {
    {core::ConstructionRules::JointFastener::Confirmat, TextId::FastenerConfirmat},
    {core::ConstructionRules::JointFastener::Eccentric, TextId::FastenerEccentric},
    {core::ConstructionRules::JointFastener::Dowel, TextId::FastenerDowel},
};
constexpr std::pair<core::ShelfMount, TextId> kShelfMounts[] = {
    {core::ShelfMount::Supports, TextId::MountSupports},
    {core::ShelfMount::Confirmat, TextId::MountConfirmat},
};
constexpr std::pair<core::HingeSide, TextId> kHinges[] = {
    {core::HingeSide::Left, TextId::Left},
    {core::HingeSide::Right, TextId::Right},
};
constexpr std::pair<core::HandleOrientation, TextId> kOrientations[] = {
    {core::HandleOrientation::Vertical, TextId::Vertical},
    {core::HandleOrientation::Horizontal, TextId::Horizontal},
};
constexpr std::pair<core::HandleAnchor, TextId> kAnchors[] = {
    {core::HandleAnchor::Top, TextId::AnchorTop},
    {core::HandleAnchor::Center, TextId::AnchorCenter},
    {core::HandleAnchor::Bottom, TextId::AnchorBottom},
};
constexpr std::pair<core::CarcassTop, TextId> kTopStyles[] = {
    {core::CarcassTop::Panel, TextId::TopStylePanel},
    {core::CarcassTop::FlatRails, TextId::TopStyleFlatRails},
    {core::CarcassTop::UprightRails, TextId::TopStyleUprightRails},
};
/// A new run on a side wall: long enough for its cabinets beyond the corner.
constexpr Real kSideRunLength = 1800.0;

constexpr std::pair<core::CabinetModule, TextId> kModules[] = {
    {core::CabinetModule::BaseDoors, TextId::ModuleBaseDoors},
    {core::CabinetModule::BaseDrawers, TextId::ModuleBaseDrawers},
    {core::CabinetModule::Sink, TextId::ModuleSink},
    {core::CabinetModule::Hob, TextId::ModuleHob},
    {core::CabinetModule::WallDoors, TextId::ModuleWallDoors},
    {core::CabinetModule::WallOpen, TextId::ModuleWallOpen},
};

std::string sizeText(Real a, Real b)
{
    return mm(a) + " × " + mm(b);
}

std::string sizeText(Real a, Real b, Real c)
{
    return mm(a) + " × " + mm(b) + " × " + mm(c);
}

// ---------------------------------------------------------------------------------------------
// Editors. Each returns true if it ran an operation: the caller must then stop drawing this
// frame (references into the model may be stale after a failed operation).
// ---------------------------------------------------------------------------------------------

bool drawProductRows(UiState& s)
{
    ed::Document& doc = *s.document;
    PropertyTable table("product");
    if (!table.isOpen()) {
        return false;
    }

    std::string name = doc.product().name();
    PropertyTable::row(tr(TextId::Name));
    ImGui::InputText("##name", &name);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        run(s, [&] { ed::setProductName(doc, name); });
        return true;
    }

    core::Size3D d = doc.product().dimensions();
    const core::Kitchen* k = doc.product().kitchen();
    const bool kitchen = k != nullptr;
    const bool corners = kitchen && k->runs().size() > 1;
    const std::pair<TextId, Real*> dims[] = {
        {kitchen ? (corners ? TextId::BackWallLength : TextId::KitchenLength) : TextId::Width, &d.width},
        {kitchen ? TextId::TotalHeight : TextId::Height, &d.height},
        {kitchen ? TextId::BaseDepth : TextId::Depth, &d.depth}};
    const Real maxLength = kitchen ? 6000.0 : 3000.0;
    const std::pair<Real, Real> limits[] = {{300.0, maxLength}, {300.0, 3000.0}, {200.0, 1000.0}};
    for (std::size_t i = 0; i < 3; ++i) {
        PropertyTable::row(tr(dims[i].first));
        ImGui::PushID(static_cast<int>(i));
        const bool changed = dragMm("##dim", *dims[i].second, limits[i].first, limits[i].second);
        const bool released = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::PopID();
        if (changed) {
            run(s, [&] { ed::setDimensions(doc, d); });
            return true;
        }
        if (released) {
            doc.endMerge(); // the drag is over: the next drag is a new undo step
        }
    }
    return false;
}

bool drawConstructionRows(UiState& s, core::Carcass& carcass)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = carcass.id();
    PropertyTable table("construction");
    if (!table.isOpen()) {
        return false;
    }

    core::CarcassTop style = carcass.topStyle();
    PropertyTable::row(tr(TextId::TopStyle));
    if (enumCombo("##topStyle", style, kTopStyles)) {
        run(s, [&] { ed::setTopStyle(doc, id, style); });
        return true;
    }
    core::JointType top = carcass.topJoint();
    ImGui::BeginDisabled(style != core::CarcassTop::Panel); // rails stand between the sides
    PropertyTable::row(tr(TextId::TopJoint));
    if (enumCombo("##top", top, kJoints)) {
        run(s, [&] { ed::setTopJoint(doc, id, top); });
        ImGui::EndDisabled();
        return true;
    }
    ImGui::EndDisabled();
    core::JointType bottom = carcass.bottomJoint();
    PropertyTable::row(tr(TextId::BottomJoint));
    if (enumCombo("##bottom", bottom, kJoints)) {
        run(s, [&] { ed::setBottomJoint(doc, id, bottom); });
        return true;
    }
    core::BackMountType back = carcass.backMountType();
    PropertyTable::row(tr(TextId::BackMount));
    if (enumCombo("##back", back, kBackMounts)) {
        run(s, [&] { ed::setBackMount(doc, id, back); });
        return true;
    }
    bool plinth = carcass.hasPlinth();
    PropertyTable::row(tr(TextId::PlinthLabel));
    if (ImGui::Checkbox("##plinth", &plinth)) {
        run(s, [&] { ed::setPlinth(doc, id, plinth); });
        return true;
    }
    return false;
}

/// A decor picked from the workshop's catalogue: a colour swatch and its name per entry.
/// `extra` (a "default" entry) comes first when given; returns true when something was picked,
/// `picked` is then the decor, or nullptr for the extra entry.
bool decorCombo(const char* id, const std::vector<core::Decor>& catalog, const std::string& current,
                const char* extra, bool extraSelected, const core::Decor*& picked)
{
    const core::Decor* now = core::findDecor(catalog, current);
    const char* preview = extraSelected ? extra : (now != nullptr ? now->name.c_str() : tr(TextId::DecorNone));
    bool changed = false;
    if (ImGui::BeginCombo(id, preview)) {
        const float swatch = ImGui::GetTextLineHeight();
        if (extra != nullptr && ImGui::Selectable(extra, extraSelected)) {
            picked = nullptr;
            changed = true;
        }
        for (const core::Decor& decor : catalog) {
            ImGui::PushID(decor.id.c_str());
            ImGui::ColorButton("##swatch", ImVec4(decor.r / 255.0f, decor.g / 255.0f, decor.b / 255.0f, 1.0f),
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(swatch, swatch));
            ImGui::SameLine();
            if (ImGui::Selectable(decor.name.c_str(), !extraSelected && &decor == now)) {
                picked = &decor;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

/// The decors of the product's materials: carcass, fronts and, in a kitchen, the worktop.
bool drawDecorRows(UiState& s)
{
    ed::Document& doc = *s.document;
    core::ConstructionRules rules = doc.product().rules();
    PropertyTable table("decors");
    if (!table.isOpen()) {
        return false;
    }
    std::vector<std::pair<TextId, core::Material*>> slots{{TextId::DecorCarcass, &rules.carcassMaterial},
                                                          {TextId::DecorFronts, &rules.frontMaterial}};
    if (doc.product().kitchen() != nullptr) {
        slots.emplace_back(TextId::DecorWorktop, &rules.worktopMaterial);
    }
    for (std::size_t i = 0; i < slots.size(); ++i) {
        PropertyTable::row(tr(slots[i].first));
        ImGui::PushID(static_cast<int>(i));
        const core::Decor* picked = nullptr;
        const bool changed = decorCombo("##decor", rules.decors, slots[i].second->decor, nullptr, false, picked);
        ImGui::PopID();
        if (changed) {
            *slots[i].second = core::withDecor(*slots[i].second, picked, rules.decors);
            run(s, [&] { ed::setRules(doc, rules); });
            return true;
        }
    }
    return false;
}

/// Kitchen sizes and the worktop height that follows from them.
bool drawKitchenRows(UiState& s)
{
    ed::Document& doc = *s.document;
    const core::Kitchen* kitchen = doc.product().kitchen();
    if (kitchen == nullptr) {
        return false;
    }
    PropertyTable table("kitchen");
    if (!table.isOpen()) {
        return false;
    }
    core::Kitchen::Sizes sizes = kitchen->sizes();
    const std::pair<TextId, Real*> rows[] = {{TextId::BaseHeight, &sizes.baseHeight},
                                             {TextId::WorktopDepth, &sizes.worktopDepth},
                                             {TextId::WallHeight, &sizes.wallHeight},
                                             {TextId::WallDepth, &sizes.wallDepth}};
    for (std::size_t i = 0; i < std::size(rows); ++i) {
        PropertyTable::row(tr(rows[i].first));
        ImGui::PushID(static_cast<int>(i));
        const bool changed = dragMm("##size", *rows[i].second, 100.0, 2000.0);
        const bool released = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::PopID();
        if (changed) {
            run(s, [&] { ed::setKitchenSizes(doc, sizes); });
            return true;
        }
        if (released) {
            doc.endMerge();
        }
    }
    PropertyTable::textRow(tr(TextId::WorktopHeight),
                           mm(kitchen->worktop().box().max().y) + mmUnit());

    // The side walls: one makes an L kitchen, both a U kitchen.
    for (const core::Side side : {core::Side::Left, core::Side::Right}) {
        const core::KitchenRun* sideRun = kitchen->sideRun(side);
        PropertyTable::row(tr(side == core::Side::Left ? TextId::RunLeft : TextId::RunRight));
        ImGui::PushID(side == core::Side::Left ? "left" : "right");
        bool present = sideRun != nullptr;
        if (ImGui::Checkbox("##present", &present)) {
            ImGui::PopID();
            const std::optional<Real> length = present ? std::optional<Real>(kSideRunLength) : std::nullopt;
            run(s, [&] { ed::setSideRun(doc, side, length); });
            return true;
        }
        if (sideRun != nullptr) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            Real length = sideRun->length();
            const bool changed = dragMm("##length", length, 600.0, 6000.0);
            const bool released = ImGui::IsItemDeactivatedAfterEdit();
            if (changed) {
                ImGui::PopID();
                run(s, [&] { ed::setSideRun(doc, side, length); });
                return true;
            }
            if (released) {
                doc.endMerge();
            }
        }
        ImGui::PopID();
    }
    return false;
}

/// A run on a side wall: its length.
bool drawRunRows(UiState& s, const core::KitchenRun& kitchenRun)
{
    if (kitchenRun.wall() == core::KitchenWall::Back) {
        return false; // as long as the product is wide: the product's own rows
    }
    ed::Document& doc = *s.document;
    PropertyTable table("run");
    if (!table.isOpen()) {
        return false;
    }
    const core::Side side = kitchenRun.wall() == core::KitchenWall::Left ? core::Side::Left : core::Side::Right;
    PropertyTable::row(tr(TextId::Length));
    Real length = kitchenRun.length();
    if (dragMm("##length", length, 600.0, 6000.0)) {
        run(s, [&] { ed::setSideRun(doc, side, length); });
        return true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        doc.endMerge();
    }
    return false;
}

/// A width or a filler of a corner cabinet; true when it was changed.
bool cornerSizeRow(UiState& s, TextId label, const core::Carcass& cabinet, bool filler)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = cabinet.id();
    PropertyTable::row(tr(label));
    ImGui::PushID(static_cast<int>(id.value * 2 + (filler ? 1 : 0)));
    Real value = filler ? cabinet.blindFiller().value_or(0.0) : cabinet.box().size().x;
    const bool changed = filler ? dragMm("##v", value, 0.0, 300.0) : dragMm("##v", value, 300.0, 2000.0);
    const bool released = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::PopID();
    if (changed) {
        run(s, [&] {
            if (filler) {
                ed::setBlindFiller(doc, id, value);
            } else {
                ed::setCabinetWidth(doc, id, value);
            }
        });
        return true;
    }
    if (released) {
        doc.endMerge();
    }
    return false;
}

/// The legs of an L corner cabinet, under one label: along the back wall and along the side wall.
bool cornerLegsRow(UiState& s, TextId label, const core::CornerCabinet& cabinet)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = cabinet.id();
    PropertyTable::row(tr(label));
    ImGui::PushID(static_cast<int>(id.value));
    Real legs[2] = {cabinet.legAlongBack(), cabinet.legAlongSide()};
    bool changed = false;
    bool released = false;
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    for (int i = 0; i < 2; ++i) {
        if (i == 1) {
            ImGui::SameLine();
        }
        ImGui::SetNextItemWidth(half);
        ImGui::PushID(i);
        changed |= dragMm("##leg", legs[i], 300.0, 2000.0);
        released |= ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            ImGui::SetTooltip("%s", tr(i == 0 ? TextId::LegAlongBack : TextId::LegAlongSide));
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    if (changed) {
        run(s, [&] { ed::setCornerLegs(doc, id, legs[0], legs[1]); });
        return true;
    }
    if (released) {
        doc.endMerge();
    }
    return false;
}

/// A kitchen corner: its type; the widths and fillers of its blind cabinets, or the legs of its
/// L cabinets.
bool drawCornerRows(UiState& s, const core::KitchenCorner& corner)
{
    ed::Document& doc = *s.document;
    PropertyTable table("corner");
    if (!table.isOpen()) {
        return false;
    }
    PropertyTable::row(tr(TextId::CornerTypeLabel));
    int type = corner.type() == core::CornerType::Blind ? 0 : 1;
    const char* types[] = {tr(TextId::CornerBlind), tr(TextId::CornerLShaped)};
    if (ImGui::Combo("##type", &type, types, 2)) {
        const core::Side end = corner.end();
        const core::CornerType chosen = type == 0 ? core::CornerType::Blind : core::CornerType::LShaped;
        run(s, [&] { ed::setCornerType(doc, end, chosen); });
        return true;
    }
    if (const core::CornerCabinet* base = corner.lCabinet(core::RowLevel::Base)) {
        return cornerLegsRow(s, TextId::BaseLegs, *base)
            || cornerLegsRow(s, TextId::WallLegs, *corner.lCabinet(core::RowLevel::Wall));
    }
    const core::Carcass* base = corner.blindCabinet(core::RowLevel::Base);
    const core::Carcass* wall = corner.blindCabinet(core::RowLevel::Wall);
    return cornerSizeRow(s, TextId::CornerBaseWidth, *base, false)
        || cornerSizeRow(s, TextId::CornerBaseFiller, *base, true)
        || cornerSizeRow(s, TextId::CornerWallWidth, *wall, false)
        || cornerSizeRow(s, TextId::CornerWallFiller, *wall, true);
}

/// An L corner cabinet: its legs and its shelves.
bool drawCornerCabinetRows(UiState& s, const core::CornerCabinet& cabinet)
{
    ed::Document& doc = *s.document;
    PropertyTable table("cornerCabinet");
    if (!table.isOpen()) {
        return false;
    }
    if (cornerLegsRow(s, TextId::KindCornerCabinet, cabinet)) {
        return true;
    }
    PropertyTable::row(tr(TextId::ShelfCount));
    int shelves = cabinet.shelfCount();
    if (ImGui::SliderInt("##shelves", &shelves, 0, core::CornerCabinet::kMaxShelves)) {
        const core::ComponentId id = cabinet.id();
        run(s, [&] { ed::setCornerShelves(doc, id, shelves); });
        return true;
    }
    return false;
}

/// "Add a cabinet": a module of the given row level (both levels if nullopt), its width, and
/// the wall it goes to (chosen here if nullopt and the kitchen has more than one).
bool drawAddCabinet(UiState& s, std::optional<core::RowLevel> level, std::optional<core::KitchenWall> wall)
{
    if (level && core::levelOf(s.newCabinet) != *level) {
        s.newCabinet = *level == core::RowLevel::Base ? core::CabinetModule::BaseDoors
                                                      : core::CabinetModule::WallDoors;
    }
    const core::Kitchen* kitchen = s.document->product().kitchen();
    if (kitchen == nullptr) {
        return false;
    }
    const std::vector<core::KitchenRun*> all = kitchen->runs();
    const std::vector<const core::KitchenRun*> runs(all.begin(), all.end());
    const auto hasRun = [&](core::KitchenWall w) {
        return std::any_of(runs.begin(), runs.end(), [w](const core::KitchenRun* r) { return r->wall() == w; });
    };
    if (!hasRun(s.newCabinetWall)) {
        s.newCabinetWall = core::KitchenWall::Back; // its run was taken away
    }
    PropertyTable table("addCabinet");
    if (!table.isOpen()) {
        return false;
    }
    if (!wall && runs.size() > 1) {
        PropertyTable::row(tr(TextId::KindKitchenRun));
        const auto wallText = [](core::KitchenWall w) {
            return tr(w == core::KitchenWall::Back ? TextId::RunBack
                      : w == core::KitchenWall::Left ? TextId::RunLeft : TextId::RunRight);
        };
        if (ImGui::BeginCombo("##wall", wallText(s.newCabinetWall))) {
            for (const core::KitchenRun* r : runs) {
                if (ImGui::Selectable(wallText(r->wall()), r->wall() == s.newCabinetWall)) {
                    s.newCabinetWall = r->wall();
                }
            }
            ImGui::EndCombo();
        }
    }
    PropertyTable::row(tr(TextId::AddCabinet));
    const char* preview = "?";
    for (const auto& [m, t] : kModules) {
        preview = m == s.newCabinet ? tr(t) : preview;
    }
    if (ImGui::BeginCombo("##module", preview)) {
        for (const auto& [m, t] : kModules) {
            if (level && core::levelOf(m) != *level) {
                continue; // a row offers only its own kind of cabinets
            }
            if (ImGui::Selectable(tr(t), m == s.newCabinet)) {
                s.newCabinet = m;
            }
        }
        ImGui::EndCombo();
    }
    PropertyTable::row(tr(TextId::Width));
    dragMm("##newWidth", s.newCabinetWidth, 150.0, 1200.0);
    PropertyTable::row("");
    if (ImGui::Button(tr(TextId::AddCabinet))) {
        const core::CabinetModule module = s.newCabinet;
        const Real width = s.newCabinetWidth;
        const core::KitchenWall target = wall.value_or(s.newCabinetWall);
        run(s, [&] { ed::addCabinet(*s.document, module, width, target); });
        return true;
    }
    return false;
}

/// A cabinet of a kitchen row: its width in the row, its place, its worktop cut-out.
bool drawCabinetRows(UiState& s, core::Carcass& cabinet)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = cabinet.id();
    const auto* row = static_cast<const core::CabinetRow*>(cabinet.parent());

    ImGui::SeparatorText(tr(TextId::KitchenHeader));
    {
        PropertyTable table("cabinet");
        if (!table.isOpen()) {
            return false;
        }
        bool fixed = cabinet.fixedWidth().has_value();
        PropertyTable::row(tr(TextId::FixedWidth));
        if (ImGui::Checkbox("##fixed", &fixed)) {
            const std::optional<Real> width =
                fixed ? std::optional<Real>(cabinet.box().size().x) : std::nullopt;
            run(s, [&] { ed::setCabinetWidth(doc, id, width); });
            return true;
        }
        if (fixed) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            Real width = *cabinet.fixedWidth();
            if (dragMm("##width", width, 100.0, 3000.0)) {
                run(s, [&] { ed::setCabinetWidth(doc, id, width); });
                return true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                doc.endMerge();
            }
        }

        if (row->level() == core::RowLevel::Base) {
            bool joint = cabinet.worktopJoint();
            PropertyTable::row(tr(TextId::WorktopJoint));
            if (ImGui::Checkbox("##joint", &joint)) {
                run(s, [&] { ed::setWorktopJoint(doc, id, joint); });
                return true;
            }
            const std::optional<core::WorktopCutout> current = cabinet.cutout();
            int kind = current ? (current->kind == core::CutoutKind::Sink ? 1 : 2) : 0;
            PropertyTable::row(tr(TextId::WorktopCutoutLabel));
            const char* kinds[] = {tr(TextId::CutoutNone), tr(TextId::CutoutSink),
                                   tr(TextId::CutoutHob)};
            if (ImGui::Combo("##cutout", &kind, kinds, 3)) {
                const std::optional<core::WorktopCutout> chosen =
                    kind == 0 ? std::nullopt
                              : std::optional(core::WorktopCutout::standard(
                                    kind == 1 ? core::CutoutKind::Sink : core::CutoutKind::Hob));
                run(s, [&] { ed::setCutout(doc, id, chosen); });
                return true;
            }
            if (current) {
                core::WorktopCutout c = *current;
                const std::pair<TextId, Real*> sizes[] = {{TextId::CutoutWidth, &c.width},
                                                          {TextId::CutoutDepth, &c.depth},
                                                          {TextId::CutoutRadius, &c.radius},
                                                          {TextId::EdgeOffset, &c.frontOffset}};
                for (std::size_t i = 0; i < std::size(sizes); ++i) {
                    PropertyTable::row(tr(sizes[i].first));
                    ImGui::PushID(static_cast<int>(i));
                    const bool changed = dragMm("##c", *sizes[i].second, 0.0, 1500.0);
                    const bool released = ImGui::IsItemDeactivatedAfterEdit();
                    ImGui::PopID();
                    if (changed) {
                        run(s, [&] { ed::setCutout(doc, id, c); });
                        return true;
                    }
                    if (released) {
                        doc.endMerge();
                    }
                }
            }
        }
    }

    ImGui::Spacing();
    if (ImGui::Button(tr(TextId::MoveLeft))) {
        run(s, [&] { ed::moveCabinet(doc, id, -1); });
        return true;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::MoveRight))) {
        run(s, [&] { ed::moveCabinet(doc, id, 1); });
        return true;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::Delete))) {
        run(s, [&] { ed::removeComponent(doc, id); });
        return true;
    }
    return false;
}

bool drawCarcassEditor(UiState& s, core::Carcass& carcass)
{
    if (drawConstructionRows(s, carcass)) {
        return true;
    }
    if (carcass.parent() != nullptr && carcass.parent()->kind() == ComponentKind::CabinetRow) {
        return drawCabinetRows(s, carcass);
    }
    if (carcass.parent() != nullptr && carcass.parent()->kind() == ComponentKind::KitchenCorner) {
        ImGui::SeparatorText(tr(TextId::KitchenHeader));
        PropertyTable table("cornerCabinet");
        return table.isOpen()
            && (cornerSizeRow(s, TextId::Width, carcass, false) || cornerSizeRow(s, TextId::Filler, carcass, true));
    }
    return false;
}

bool drawProfileRows(UiState& s)
{
    ed::Document& doc = *s.document;
    PropertyTable table("profile");
    if (!table.isOpen()) {
        return false;
    }
    core::ConstructionRules rules = doc.product().rules();
    bool changed = false;

    PropertyTable::row(tr(TextId::Fastener));
    changed |= enumCombo("##fastener", rules.joints.fastener, kFasteners);
    PropertyTable::row(tr(TextId::AlignmentDowels));
    changed |= ImGui::Checkbox("##dowels", &rules.joints.alignmentDowels);
    PropertyTable::row(tr(TextId::DrillHandlesOnMachine));
    changed |= ImGui::Checkbox("##handles", &rules.handles.drillOnMachine);
    if (doc.product().kitchen() != nullptr) {
        PropertyTable::row(tr(TextId::PlinthHeight));
        changed |= dragMm("##plinth", rules.kitchen.plinthHeight, 50.0, 300.0);
        PropertyTable::row(tr(TextId::PlinthSetback));
        changed |= dragMm("##setback", rules.kitchen.plinthSetback, 0.0, 150.0);
        PropertyTable::row(tr(TextId::RailWidth));
        changed |= dragMm("##rail", rules.kitchen.railWidth, 50.0, 200.0);
        PropertyTable::row(tr(TextId::WorktopMaxLength));
        changed |= dragMm("##slab", rules.kitchen.worktopMaxLength, 1000.0, 6000.0);
    }

    if (changed) {
        run(s, [&] { ed::setRules(doc, rules); });
        return true;
    }
    return false;
}

core::HandlePlacement defaultPlacement(ComponentKind kind)
{
    core::HandlePlacement p;
    p.orientation = kind == ComponentKind::Door ? core::HandleOrientation::Vertical
                                                : core::HandleOrientation::Horizontal;
    return p;
}

bool drawHandleRows(UiState& s, core::FrontPanel& front)
{
    ed::Document& doc = *s.document;
    const core::ConstructionRules& rules = doc.product().rules();
    const core::ComponentId id = front.id();
    const bool isDoor = front.kind() == ComponentKind::Door;
    const std::optional<core::HandlePlacement> current = front.handle();

    const std::string& defaultId =
        isDoor ? rules.handles.defaultDoorHandle : rules.handles.defaultDrawerHandle;
    const core::HandleModel* defaultModel = rules.handles.find(defaultId);
    const std::string defaultLabel = std::string(tr(TextId::HandleDefault)) + " ("
                                   + (defaultModel != nullptr ? defaultModel->name : defaultId)
                                   + ")";

    std::string preview = tr(TextId::HandleNone);
    if (current) {
        if (current->modelId.empty()) {
            preview = defaultLabel;
        } else {
            const core::HandleModel* model = rules.handles.find(current->modelId);
            preview = model != nullptr ? model->name : current->modelId;
        }
    }

    PropertyTable table("handle");
    if (!table.isOpen()) {
        return false;
    }
    PropertyTable::row(tr(TextId::Handle));
    std::optional<std::optional<core::HandlePlacement>> chosen;
    if (ImGui::BeginCombo("##model", preview.c_str())) {
        if (ImGui::Selectable(tr(TextId::HandleNone), !current)) {
            chosen = std::optional<core::HandlePlacement>{};
        }
        core::HandlePlacement base = current.value_or(defaultPlacement(front.kind()));
        if (ImGui::Selectable(defaultLabel.c_str(), current && current->modelId.empty())) {
            base.modelId.clear();
            chosen = base;
        }
        for (const core::HandleModel& model : rules.handles.catalog) {
            const bool selected = current && current->modelId == model.id;
            if (ImGui::Selectable(model.name.c_str(), selected)) {
                base.modelId = model.id;
                chosen = base;
            }
        }
        ImGui::EndCombo();
    }
    if (chosen) {
        run(s, [&] { ed::setHandle(doc, id, *chosen); });
        return true;
    }
    if (!current) {
        return false;
    }

    core::HandlePlacement p = *current;
    bool changed = false;
    bool released = false;
    PropertyTable::row(tr(TextId::Orientation));
    changed |= enumCombo("##orientation", p.orientation, kOrientations);
    PropertyTable::row(tr(TextId::Anchor));
    changed |= enumCombo("##anchor", p.anchor, kAnchors);
    if (isDoor) {
        PropertyTable::row(tr(TextId::EdgeOffset));
        changed |= dragMm("##edge", p.edgeOffset, 0.0, 500.0);
        released |= ImGui::IsItemDeactivatedAfterEdit();
    }
    if (p.anchor != core::HandleAnchor::Center) {
        PropertyTable::row(tr(TextId::AnchorOffset));
        changed |= dragMm("##anchorOffset", p.anchorOffset, 0.0, 1000.0);
        released |= ImGui::IsItemDeactivatedAfterEdit();
    }
    if (changed) {
        run(s, [&] { ed::setHandle(doc, id, p); });
        return true;
    }
    if (released) {
        doc.endMerge();
    }
    return false;
}

bool deleteButton(UiState& s, core::ComponentId id)
{
    ImGui::Spacing();
    if (ImGui::Button(tr(TextId::Delete))) {
        run(s, [&] { ed::removeComponent(*s.document, id); });
        return true;
    }
    return false;
}

bool drawSectionEditor(UiState& s, core::Section& section)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = section.id();

    // Size along the parent's split axis (only for sub-sections).
    if (const auto* parent = dynamic_cast<const core::Section*>(section.parent())) {
        PropertyTable table("size");
        if (table.isOpen()) {
            const bool alongX = parent->splitAxis() == core::Axis::X;
            const Real current = alongX ? section.size().width : section.size().height;
            bool fixed = section.fixedSize().has_value();
            PropertyTable::row(tr(alongX ? TextId::FixedWidth : TextId::FixedHeight));
            if (ImGui::Checkbox("##fixed", &fixed)) {
                const std::optional<Real> size = fixed ? std::optional<Real>(current) : std::nullopt;
                run(s, [&] { ed::setSectionFixedSize(doc, id, size); });
                return true;
            }
            if (fixed) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                Real value = *section.fixedSize();
                if (dragMm("##fixedSize", value, 50.0, 3000.0)) {
                    run(s, [&] { ed::setSectionFixedSize(doc, id, value); });
                    return true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    doc.endMerge();
                }
            }
        }
    }

    const bool split = section.isSplit();
    const bool hasDrawers = section.drawerStack() != nullptr;
    const auto shelves = section.shelves();

    ImGui::SeparatorText(tr(TextId::Content));
    {
        PropertyTable table("content");
        if (table.isOpen()) {
            if (!split) {
                int autoShelves = 0;
                core::ShelfMount mount = core::ShelfMount::Supports;
                for (const core::Shelf* shelf : shelves) {
                    if (!shelf->elevation()) {
                        mount = autoShelves == 0 ? shelf->mount() : mount;
                        ++autoShelves;
                    }
                }
                ImGui::BeginDisabled(hasDrawers);
                PropertyTable::row(tr(TextId::ShelfCount));
                int count = autoShelves;
                if (ImGui::InputInt("##shelves", &count) && count != autoShelves) {
                    count = std::clamp(count, 0, 20);
                    run(s, [&] { ed::setShelfCount(doc, id, count, mount); });
                    ImGui::EndDisabled();
                    return true;
                }
                PropertyTable::row(tr(TextId::ShelfMountLabel));
                if (enumCombo("##mount", mount, kShelfMounts)) {
                    run(s, [&] { ed::setShelfCount(doc, id, autoShelves, mount); });
                    ImGui::EndDisabled();
                    return true;
                }
                ImGui::EndDisabled();

                const bool drawersAllowed = shelves.empty() && section.doors().empty();
                ImGui::BeginDisabled(!drawersAllowed && !hasDrawers);
                PropertyTable::row(tr(TextId::DrawerCount));
                const int drawers = hasDrawers ? section.drawerStack()->count() : 0;
                int drawerCount = drawers;
                if (ImGui::InputInt("##drawers", &drawerCount) && drawerCount != drawers) {
                    drawerCount = std::clamp(drawerCount, 0, 10);
                    run(s, [&] { ed::setDrawerCount(doc, id, drawerCount); });
                    ImGui::EndDisabled();
                    return true;
                }
                ImGui::EndDisabled();
            }

            ImGui::BeginDisabled(hasDrawers);
            PropertyTable::row(tr(TextId::DoorCount));
            const int doors = static_cast<int>(section.doors().size());
            int doorCount = doors;
            if (ImGui::SliderInt("##doors", &doorCount, 0, 2) && doorCount != doors) {
                run(s, [&] { ed::setDoorCount(doc, id, doorCount); });
                ImGui::EndDisabled();
                return true;
            }
            ImGui::EndDisabled();
        }
    }

    ImGui::SeparatorText(tr(TextId::Splitting));
    const bool canSplit = !split && shelves.empty() && !hasDrawers;
    ImGui::BeginDisabled(!canSplit);
    for (const auto& [axis, label] : {std::pair{core::Axis::X, TextId::SplitColumns},
                                      std::pair{core::Axis::Y, TextId::SplitRows}}) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(label));
        for (int n = 2; n <= 4; ++n) {
            ImGui::SameLine();
            ImGui::PushID(static_cast<int>(core::index(axis)) * 10 + n);
            const bool clicked = ImGui::Button(std::to_string(n).c_str());
            ImGui::PopID();
            if (clicked) {
                run(s, [&] { ed::splitSection(doc, id, axis, n); });
                ImGui::EndDisabled();
                return true;
            }
        }
    }
    ImGui::EndDisabled();
    if (!section.children().empty() && ImGui::Button(tr(TextId::ClearSection))) {
        run(s, [&] { ed::clearSection(doc, id); });
        return true;
    }
    return false;
}

bool drawShelfEditor(UiState& s, core::Shelf& shelf)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = shelf.id();
    {
        PropertyTable table("shelf");
        if (table.isOpen()) {
            core::ShelfMount mount = shelf.mount();
            PropertyTable::row(tr(TextId::ShelfMountLabel));
            if (enumCombo("##mount", mount, kShelfMounts)) {
                run(s, [&] { ed::setShelfMount(doc, id, mount); });
                return true;
            }
            const Real sectionBottom =
                shelf.parent() != nullptr ? shelf.parent()->box().min().y : 0.0;
            bool fixed = shelf.elevation().has_value();
            PropertyTable::row(tr(TextId::FixedElevation));
            if (ImGui::Checkbox("##fixed", &fixed)) {
                const std::optional<Real> elevation =
                    fixed ? std::optional<Real>(shelf.box().min().y - sectionBottom) : std::nullopt;
                run(s, [&] { ed::setShelfElevation(doc, id, elevation); });
                return true;
            }
            if (fixed) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                Real value = *shelf.elevation();
                if (dragMm("##elevation", value, 0.0, 3000.0)) {
                    run(s, [&] { ed::setShelfElevation(doc, id, value); });
                    return true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    doc.endMerge();
                }
            }
        }
    }
    return deleteButton(s, id);
}

bool drawDoorEditor(UiState& s, core::Door& door)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = door.id();
    {
        PropertyTable table("door");
        if (table.isOpen()) {
            core::HingeSide hinge = door.hinge();
            PropertyTable::row(tr(TextId::HingeSideLabel));
            if (enumCombo("##hinge", hinge, kHinges)) {
                run(s, [&] { ed::setHinge(doc, id, hinge); });
                return true;
            }
        }
    }
    if (drawHandleRows(s, door)) {
        return true;
    }
    return deleteButton(s, id);
}

bool drawDrawerStackEditor(UiState& s, core::DrawerStack& stack)
{
    ed::Document& doc = *s.document;
    const core::FurnitureComponent* section = stack.parent();
    {
        PropertyTable table("drawers");
        if (table.isOpen() && section != nullptr) {
            const core::ComponentId sectionId = section->id();
            PropertyTable::row(tr(TextId::DrawerCount));
            int count = stack.count();
            if (ImGui::InputInt("##count", &count) && count != stack.count()) {
                count = std::clamp(count, 0, 10);
                run(s, [&] { ed::setDrawerCount(doc, sectionId, count); });
                return true;
            }
        }
    }
    return deleteButton(s, stack.id());
}

// ---------------------------------------------------------------------------------------------
// Shape of a part: notches, shaped corners, openings
// ---------------------------------------------------------------------------------------------

constexpr core::PanelCorner kCorners[] = {core::PanelCorner::L1W1, core::PanelCorner::L1W2,
                                          core::PanelCorner::L2W2, core::PanelCorner::L2W1};
constexpr core::EdgeSide kEdges[] = {core::EdgeSide::L1, core::EdgeSide::L2, core::EdgeSide::W1,
                                     core::EdgeSide::W2};

bool cornerCombo(const char* id, core::PanelCorner& corner)
{
    bool changed = false;
    if (ImGui::BeginCombo(id, cornerText(corner).c_str())) {
        for (const core::PanelCorner c : kCorners) {
            if (ImGui::Selectable(cornerText(c).c_str(), c == corner) && c != corner) {
                corner = c;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool edgeCombo(const char* id, core::EdgeSide& edge)
{
    bool changed = false;
    if (ImGui::BeginCombo(id, tr(edgeText(edge)))) {
        for (const core::EdgeSide e : kEdges) {
            if (ImGui::Selectable(tr(edgeText(e)), e == edge) && e != edge) {
                edge = e;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

/// A new feature of the given kind that fits a part of this size: at a corner, or centred.
core::ShapeFeature newFeature(std::size_t kind, Real length, Real width)
{
    const Real small = std::min(length, width);
    switch (kind) {
    case 0:
        return core::CornerNotch{core::PanelCorner::L1W1, std::min(100.0, length / 3.0),
                                 std::min(20.0, width / 3.0)};
    case 1:
        return core::CornerRound{core::PanelCorner::L1W2, std::min(50.0, small / 3.0)};
    case 2:
        return core::CornerChamfer{core::PanelCorner::L1W2, std::min(50.0, length / 3.0),
                                   std::min(50.0, width / 3.0)};
    case 3: {
        const Real notch = std::min(100.0, length / 4.0);
        return core::EdgeNotch{core::EdgeSide::L1, false, (length - notch) / 2.0, notch,
                               std::min(30.0, width / 3.0)};
    }
    case 4:
        return core::InnerCutout{core::PanelCorner::L1W1, length / 2.0, width / 2.0,
                                 std::min(100.0, length / 3.0), std::min(60.0, width / 3.0), 5.0};
    default:
        return core::RoundHole{core::PanelCorner::L1W1, length / 2.0, width / 2.0,
                               std::min(60.0, small / 3.0)};
    }
}

/// The fields of one feature. Sets `released` when a drag ended (the next one is a new step).
bool featureRows(core::ShapeFeature& feature, bool& released)
{
    bool changed = false;
    const auto size = [&](TextId label, Real& value, const char* id) {
        PropertyTable::row(tr(label));
        changed |= dragMm(id, value, 0.0, 5000.0);
        released |= ImGui::IsItemDeactivatedAfterEdit();
    };
    std::visit(
        [&](auto& f) {
            using T = std::decay_t<decltype(f)>;
            if constexpr (std::is_same_v<T, core::CornerNotch> || std::is_same_v<T, core::CornerChamfer>) {
                PropertyTable::row(tr(TextId::ShapeCornerLabel));
                changed |= cornerCombo("##corner", f.corner);
                size(TextId::ShapeAlongLength, f.alongLength, "##l");
                size(TextId::ShapeAlongWidth, f.alongWidth, "##w");
            } else if constexpr (std::is_same_v<T, core::CornerRound>) {
                PropertyTable::row(tr(TextId::ShapeCornerLabel));
                changed |= cornerCombo("##corner", f.corner);
                size(TextId::ShapeRadius, f.radius, "##r");
            } else if constexpr (std::is_same_v<T, core::EdgeNotch>) {
                PropertyTable::row(tr(TextId::ShapeEdgeLabel));
                changed |= edgeCombo("##edge", f.edge);
                PropertyTable::row(tr(TextId::ShapeFromEnd));
                changed |= ImGui::Checkbox("##fromEnd", &f.fromEnd);
                size(TextId::ShapeOffset, f.offset, "##o");
                size(TextId::ShapeNotchLength, f.length, "##l");
                size(TextId::ShapeNotchDepth, f.depth, "##d");
            } else if constexpr (std::is_same_v<T, core::InnerCutout>) {
                PropertyTable::row(tr(TextId::ShapeFromLabel));
                changed |= cornerCombo("##from", f.from);
                size(TextId::ShapeAlongLength, f.alongLength, "##x");
                size(TextId::ShapeAlongWidth, f.alongWidth, "##y");
                size(TextId::ShapeOpeningLength, f.length, "##l");
                size(TextId::ShapeOpeningWidth, f.width, "##w");
                size(TextId::ShapeRadius, f.radius, "##r");
            } else {
                PropertyTable::row(tr(TextId::ShapeFromLabel));
                changed |= cornerCombo("##from", f.from);
                size(TextId::ShapeAlongLength, f.alongLength, "##x");
                size(TextId::ShapeAlongWidth, f.alongWidth, "##y");
                size(TextId::ShapeDiameter, f.diameter, "##d");
            }
        },
        feature);
    return changed;
}

/// The part as its drawing shows it: outline, openings, the names of its edges.
void drawPartSketch(const ed::Document& doc, const core::PanelComponent& part)
{
    const detailing::FlatPart flat = detailing::flatten(*part.panel(), doc.product().rules());
    const float width = ImGui::GetContentRegionAvail().x;
    const float margin = ImGui::GetTextLineHeight() * 1.6f;
    const float height = std::clamp(width * 0.45f, 90.0f, 160.0f);
    const float scale = std::min((width - 2.0f * margin) / static_cast<float>(flat.length),
                                 (height - 2.0f * margin) / static_cast<float>(flat.width));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 base(origin.x + (width - scale * static_cast<float>(flat.length)) / 2.0f,
                      origin.y + (height + scale * static_cast<float>(flat.width)) / 2.0f);
    const auto at = [&](core::Vec2 p) {
        return ImVec2(base.x + scale * static_cast<float>(p.x), base.y - scale * static_cast<float>(p.y));
    };
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 line = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 band = IM_COL32(220, 70, 60, 255);
    for (const detailing::FlatContourSide& side : flat.contour) {
        draw->AddLine(at(side.start), at(side.end), side.band.present() ? band : line, 2.0f);
    }
    for (const detailing::FlatCutout& c : flat.cutouts) {
        std::vector<ImVec2> points;
        for (const core::Vec2 p : detailing::cutoutOutline(c)) {
            points.push_back(at(p));
        }
        draw->AddPolyline(points.data(), static_cast<int>(points.size()), line, 1.5f, ImDrawFlags_Closed);
    }
    // Edge names outside the part, as in the drawings.
    const ImU32 label = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    for (std::size_t i = 0; i < flat.outline.size(); ++i) {
        const detailing::FlatEdge& edge = flat.outline[i];
        const core::Vec2 along = glm::normalize(edge.end - edge.start);
        const core::Vec2 outside(along.y, -along.x);
        const ImVec2 mid = at((edge.start + edge.end) * 0.5);
        const char* name = tr(edgeText(edge.side));
        const ImVec2 textSize = ImGui::CalcTextSize(name);
        const float push = margin * 0.55f;
        draw->AddText(ImVec2(mid.x + static_cast<float>(outside.x) * push - textSize.x / 2.0f,
                             mid.y - static_cast<float>(outside.y) * push - textSize.y / 2.0f),
                      label, name);
    }
    ImGui::Dummy(ImVec2(width, height));
}

bool drawShapeEditor(UiState& s, core::PanelComponent& part)
{
    ed::Document& doc = *s.document;
    const core::ComponentId id = part.id();
    ImGui::SeparatorText(tr(TextId::ShapeHeader));
    drawPartSketch(doc, part);

    // Openings the part gets from where it stands (a worktop's sink): shown, not edited here.
    const std::size_t own = core::featureOpenings(part.panel()->finishLength(),
                                                  part.panel()->finishWidth(), part.shape())
                                .size();
    const auto& all = part.panel()->cutouts();
    for (std::size_t i = 0; i + own < all.size(); ++i) {
        ImGui::TextDisabled("%s %s × %s (%s)", tr(TextId::ShapeInnerCutout), mm(all[i].length).c_str(),
                            mm(all[i].width).c_str(), tr(TextId::ShapeAutomatic));
    }

    std::vector<core::ShapeFeature> features = part.shape();
    bool changed = false;
    bool released = false;
    std::optional<std::size_t> removed;
    for (std::size_t i = 0; i < features.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(featureKindText(features[i])));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(tr(TextId::Delete)).x);
        if (ImGui::SmallButton(tr(TextId::Delete))) {
            removed = i;
        }
        {
            PropertyTable table("feature");
            if (table.isOpen()) {
                changed |= featureRows(features[i], released);
            }
        }
        ImGui::PopID();
    }
    if (removed) {
        features.erase(features.begin() + static_cast<std::ptrdiff_t>(*removed));
        run(s, [&] { ed::setShape(doc, id, features); });
        return true;
    }
    if (changed) {
        run(s, [&] { ed::setShape(doc, id, features); });
        return true;
    }
    if (released) {
        doc.endMerge();
    }

    static constexpr TextId kKinds[] = {TextId::ShapeCornerNotch, TextId::ShapeCornerRound,
                                        TextId::ShapeChamfer,     TextId::ShapeEdgeNotch,
                                        TextId::ShapeInnerCutout, TextId::ShapeRoundHole};
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##addShape", tr(TextId::ShapeAdd))) {
        for (std::size_t k = 0; k < std::size(kKinds); ++k) {
            if (ImGui::Selectable(tr(kKinds[k]))) {
                features.push_back(newFeature(k, part.panel()->finishLength(), part.panel()->finishWidth()));
                ImGui::EndCombo();
                run(s, [&] { ed::setShape(doc, id, features); });
                return true;
            }
        }
        ImGui::EndCombo();
    }
    return false;
}

/// Kind-specific parameters of the selected component.
bool drawEditor(UiState& s, core::FurnitureComponent& c)
{
    switch (c.kind()) {
    case ComponentKind::Product:
        return drawProductRows(s);
    case ComponentKind::Carcass:
        return drawCarcassEditor(s, static_cast<core::Carcass&>(c));
    case ComponentKind::Kitchen:
        return drawKitchenRows(s) || drawAddCabinet(s, std::nullopt, std::nullopt);
    case ComponentKind::KitchenRun: {
        const auto& kitchenRun = static_cast<core::KitchenRun&>(c);
        return drawRunRows(s, kitchenRun) || drawAddCabinet(s, std::nullopt, kitchenRun.wall());
    }
    case ComponentKind::KitchenCorner:
        return drawCornerRows(s, static_cast<core::KitchenCorner&>(c));
    case ComponentKind::CabinetRow:
        return drawAddCabinet(s, static_cast<core::CabinetRow&>(c).level(),
                              static_cast<const core::KitchenRun*>(c.parent())->wall());
    case ComponentKind::Section:
        return drawSectionEditor(s, static_cast<core::Section&>(c));
    case ComponentKind::Shelf:
        return drawShelfEditor(s, static_cast<core::Shelf&>(c));
    case ComponentKind::Door:
        // The doors of an L corner cabinet fold together: only their handles change.
        if (c.parent() != nullptr && c.parent()->kind() == ComponentKind::CornerCabinet) {
            return drawHandleRows(s, static_cast<core::FrontPanel&>(c));
        }
        return drawDoorEditor(s, static_cast<core::Door&>(c));
    case ComponentKind::CornerCabinet:
        return drawCornerCabinetRows(s, static_cast<core::CornerCabinet&>(c));
    case ComponentKind::DrawerFront:
        return drawHandleRows(s, static_cast<core::FrontPanel&>(c));
    case ComponentKind::DrawerStack:
        return drawDrawerStackEditor(s, static_cast<core::DrawerStack&>(c));
    default:
        return false;
    }
}

void drawInfo(const ed::Document& doc, const core::FurnitureComponent& c)
{
    const ed::PartInfo info = ed::describe(c, doc.product().rules());
    ImGui::SeparatorText(tr(TextId::Info));
    PropertyTable table("info");
    if (!table.isOpen()) {
        return;
    }
    PropertyTable::textRow(tr(TextId::Size),
                           sizeText(info.size.width, info.size.height, info.size.depth) + mmUnit());
    if (!info.panel) {
        return;
    }
    const ed::PanelInfo& p = *info.panel;
    PropertyTable::textRow(tr(TextId::Material), p.material.name);
    PropertyTable::textRow(tr(TextId::Thickness), mm(p.thickness) + mmUnit());
    PropertyTable::textRow(tr(TextId::FinishSize), sizeText(p.finishLength, p.finishWidth) + mmUnit());
    PropertyTable::textRow(tr(TextId::CutSize), sizeText(p.cut.length, p.cut.width) + mmUnit());

    std::string edges;
    for (std::size_t i = 0; i < p.edges.size(); ++i) {
        edges += std::string(i > 0 ? "  " : "") + tr(edgeText(static_cast<core::EdgeSide>(i))) + ": "
               + (p.edges[i].present() ? p.edges[i].name : std::string(tr(TextId::NoEdge)));
    }
    PropertyTable::row(tr(TextId::EdgeBanding));
    ImGui::TextWrapped("%s", edges.c_str());
    PropertyTable::textRow(tr(TextId::Grain),
                           tr(p.material.hasGrain ? TextId::GrainKept : TextId::GrainFree));

    std::string holes = std::to_string(p.holeCount);
    for (const auto& [type, count] : p.holes) {
        holes += "\n  " + std::string(tr(holeText(type))) + ": " + std::to_string(count);
    }
    PropertyTable::textRow(tr(TextId::Holes), holes);
    PropertyTable::textRow(tr(TextId::Grooves), std::to_string(p.grooveCount));
}

void drawTreeNode(UiState& s, core::FurnitureComponent& c)
{
    ed::Document& doc = *s.document;
    const core::FurnitureComponent* selected = doc.selected();
    const bool isSelected = selected == &c;
    const bool isLeaf = c.children().empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                             | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (isLeaf) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    // A wardrobe opens down to its sections; a kitchen down to its cabinets (dozens of them).
    bool inKitchen = false;
    for (const core::FurnitureComponent* p = c.parent(); p != nullptr; p = p->parent()) {
        inKitchen = inKitchen || p->kind() == ComponentKind::Kitchen;
    }
    const bool openByDefault =
        c.kind() == ComponentKind::Product || c.kind() == ComponentKind::Kitchen
        || c.kind() == ComponentKind::KitchenRun || c.kind() == ComponentKind::CabinetRow
        || (!inKitchen && (c.kind() == ComponentKind::Carcass || c.kind() == ComponentKind::Section));
    if (openByDefault) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }
    if (s.scrollTreeToSelection && selected != nullptr && c.isAncestorOf(*selected)) {
        ImGui::SetNextItemOpen(true); // unfold down to a part picked in the 3D view
    }

    ImGui::PushID(&c);
    const bool open = ImGui::TreeNodeEx("node", flags, "%s", displayName(c).c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        doc.select(c.id());
    }
    if (isSelected && s.scrollTreeToSelection) {
        ImGui::SetScrollHereY(0.5f);
        s.scrollTreeToSelection = false;
    }
    if (open && !isLeaf) {
        for (const auto& child : c.children()) {
            drawTreeNode(s, *child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void drawProperties(UiState& s)
{
    ed::Document& doc = *s.document;
    core::FurnitureComponent* selected = doc.selected();
    if (selected == nullptr) {
        ImGui::TextWrapped("%s", tr(TextId::SelectAPart));
        return;
    }
    ImGui::TextUnformatted(displayName(*selected).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", tr(kindText(selected->kind())));
    ImGui::Separator();

    if (drawEditor(s, *selected)) {
        return; // the model changed: draw the fresh state next frame
    }
    if (auto* panel = dynamic_cast<core::PanelComponent*>(selected)) {
        bool inPlan = panel->inCuttingPlan();
        if (ImGui::Checkbox(tr(TextId::InCuttingPlan), &inPlan)) {
            const core::ComponentId id = panel->id();
            run(s, [&] { ed::setInCuttingPlan(doc, {id}, inPlan); });
            return;
        }
        // Its own decor, or the one of its material in the product.
        {
            PropertyTable table("partDecor");
            if (table.isOpen()) {
                PropertyTable::row(tr(TextId::DecorLabel));
                const core::ConstructionRules& rules = doc.product().rules();
                const bool own = panel->materialOverride().has_value();
                const std::string defaultLabel = std::string(tr(TextId::DecorDefault)) + " ("
                                               + panel->material().name + ")";
                const core::Decor* picked = nullptr;
                if (decorCombo("##decor", rules.decors, panel->material().decor,
                               own ? defaultLabel.c_str() : defaultLabel.c_str(), !own, picked)) {
                    const core::ComponentId id = panel->id();
                    const std::optional<core::Material> material =
                        picked == nullptr ? std::nullopt
                                          : std::optional(core::withDecor(panel->material(), picked, rules.decors));
                    run(s, [&] { ed::setMaterialOverride(doc, id, material); });
                    return;
                }
            }
        }
        if (drawShapeEditor(s, *panel)) {
            return;
        }
    }
    drawInfo(doc, *selected);
}

void ensureScene(UiState& s)
{
    const ed::Document& doc = *s.document;
    const core::FurnitureComponent* selected = doc.selected();
    const std::optional<core::ComponentId> selection =
        selected != nullptr ? std::optional(selected->id()) : std::nullopt;
    if (s.sceneValid && s.sceneRevision == doc.revision() && s.sceneOptions.selection == selection) {
        return;
    }
    s.sceneOptions.selection = selection;
    s.scene = render::buildScene(doc.product(), s.sceneOptions);
    s.sceneRevision = doc.revision();
    s.sceneValid = true;
}

void handleViewportInput(UiState& s, const ImVec2& origin, const ImVec2& size)
{
    ImGuiIO& io = ImGui::GetIO();
    const glm::vec2 viewport(size.x, size.y);

    if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
        s.camera.zoom(io.MouseWheel);
    }
    if (ImGui::IsItemHovered() && s.dragButton < 0) {
        for (int button = 0; button < 3; ++button) {
            if (ImGui::IsMouseClicked(button)) {
                s.dragButton = button;
                s.dragStart = io.MousePos;
                s.dragMoved = false;
                break;
            }
        }
    }
    if (s.dragButton < 0) {
        return;
    }
    if (ImGui::IsMouseDown(s.dragButton)) {
        const float dx = io.MousePos.x - s.dragStart.x;
        const float dy = io.MousePos.y - s.dragStart.y;
        if (dx * dx + dy * dy > 16.0f) {
            s.dragMoved = true;
        }
        if (s.dragMoved) {
            if (s.dragButton == ImGuiMouseButton_Left) {
                s.camera.orbit(io.MouseDelta.x, io.MouseDelta.y);
            } else {
                s.camera.pan(io.MouseDelta.x, io.MouseDelta.y, viewport);
            }
        }
        return;
    }
    // Released: a click without dragging selects what is under the mouse.
    if (!s.dragMoved && s.dragButton == ImGuiMouseButton_Left) {
        const glm::vec2 pixel(s.dragStart.x - origin.x, s.dragStart.y - origin.y);
        const render::Ray ray = s.camera.rayThrough(pixel, viewport);
        if (const auto hit = render::pick(s.scene, ray)) {
            s.document->select(*hit);
            s.scrollTreeToSelection = true;
        } else {
            s.document->clearSelection();
        }
    }
    s.dragButton = -1;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Document-level actions
// ---------------------------------------------------------------------------------------------

bool run(UiState& state, const std::function<void()>& operation)
{
    try {
        operation();
        return true;
    } catch (const std::exception& e) {
        state.errorMessage = e.what();
        state.openErrorPopup = true;
        return false;
    }
}

void confirmUnsaved(UiState& state, std::function<void()> action)
{
    if (!state.document || !state.document->isDirty()) {
        action();
        return;
    }
    state.pendingAction = std::move(action);
    state.openUnsavedPopup = true;
}

bool saveDocument(UiState& state, bool forceDialog)
{
    ed::Document& doc = *state.document;
    if (forceDialog || doc.path().empty()) {
        const std::string name =
            doc.product().name().empty() ? tr(TextId::Untitled) : doc.product().name();
        const auto path = showSaveDialog(FileKind::Project, name + ".rcproj");
        if (!path) {
            return false;
        }
        return run(state, [&] { doc.saveAs(*path); });
    }
    return run(state, [&] { doc.save(); });
}

void setDocument(UiState& state, std::unique_ptr<ed::Document> document)
{
    state.document = std::move(document);
    ++state.documentSerial;
    state.fitRequested = true;
    state.sceneValid = false;
    // A cutting plan belongs to its product: the new document starts without one.
    state.nestingRun.reset();
    state.nesting.reset();
    ++state.nestingChanges;
    state.nestingMessage.clear();
}

void newDocument(UiState& state, core::CatalogItem item)
{
    const core::ConstructionRules rules =
        state.document ? state.document->product().rules() : core::ConstructionRules{};
    run(state, [&] {
        setDocument(state, std::make_unique<ed::Document>(ed::Document::fromCatalog(item, rules)));
    });
}

void openDocument(UiState& state)
{
    const auto path = showOpenDialog(FileKind::Project);
    if (!path) {
        return;
    }
    run(state, [&] { setDocument(state, std::make_unique<ed::Document>(ed::Document::open(*path))); });
}

void loadProfile(UiState& state)
{
    const auto path = showOpenDialog(FileKind::Profile);
    if (!path) {
        return;
    }
    run(state, [&] {
        ed::setRules(*state.document, storage::loadProfile(storage::readTextFile(*path)));
    });
}

void saveProfile(UiState& state)
{
    const auto path = showSaveDialog(FileKind::Profile, "atolye.rcprofile");
    if (!path) {
        return;
    }
    run(state, [&] {
        storage::writeTextFile(*path, storage::saveProfile(state.document->product().rules()));
    });
}

// ---------------------------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------------------------

namespace {

/// A word for a file name in the documents' language ("kesim_listesi", "spisok_detaley").
std::string documentWord(const UiState& state, TextId id)
{
    const i18n::LanguageScope inDocuments(documentLanguage(state));
    return tr(id);
}

/// After a successful export: open the result right away or say where it is.
void exported(UiState& state, std::string message, const std::filesystem::path& file, bool openNow)
{
    if (openNow && openWithSystem(file)) {
        return;
    }
    state.exportMessage = std::move(message);
    state.exportedFile = file;
    state.openExportedPopup = true;
}

/// The order of the open document: as many copies as the cutting plan is made for.
production::Order orderOf(const UiState& state)
{
    return {*state.document, state.productCount};
}

/// The cutting plan if it belongs to the product as it is now, else null.
const nesting::NestingResult* freshPlan(const UiState& state)
{
    return nestingIsFresh(state) ? &*state.nesting : nullptr;
}

const exporting::PdfFonts& pdfFonts(UiState& state)
{
    if (!state.pdfFonts) {
        state.pdfFonts = exporting::findPdfFonts(); // looked up once
    }
    return *state.pdfFonts;
}

void writePdf(UiState& state)
{
    const auto path = showSaveDialog(FileKind::Pdf, production::orderName(*state.document) + ".pdf");
    if (path && writePdfFile(state, *path)) {
        exported(state, tr(TextId::ExportDoneFile), *path, state.openAfterExport);
    }
}

} // namespace

i18n::Language documentLanguage(const UiState& state) noexcept
{
    return state.documentLanguage.value_or(i18n::currentLanguage());
}

bool writePdfFile(UiState& state, const std::filesystem::path& path)
{
    // Only a plan that belongs to this product as it is now goes into the documents.
    const exporting::PdfFonts& fonts = pdfFonts(state);
    return run(state, [&] {
        const i18n::LanguageScope inDocuments(documentLanguage(state));
        production::writeDocumentation(orderOf(state), freshPlan(state), state.pdfOptions, fonts,
                                       path);
    });
}

void exportPdf(UiState& state)
{
    state.openPdfPopup = true; // settings first, see drawPopups()
}

void exportPartsDxf(UiState& state)
{
    const auto path = showSaveDialog(FileKind::Dxf, production::orderName(*state.document) + ".dxf");
    if (path && run(state, [&] {
            const i18n::LanguageScope inDocuments(documentLanguage(state));
            production::writePartsDxf(orderOf(state), *path);
        })) {
        exported(state, tr(TextId::ExportDoneFile), *path, false);
    }
}

void exportPartsDxfFolder(UiState& state)
{
    const auto folder = showFolderDialog();
    if (!folder) {
        return;
    }
    // A folder of its own, ASCII-named like the files: CNC software and USB sticks cope with it.
    const std::filesystem::path target =
        *folder / (safeFileName(production::orderName(*state.document)) + "_DXF"); // DXF in any language
    std::size_t count = 0;
    if (run(state, [&] {
            const i18n::LanguageScope inDocuments(documentLanguage(state));
            count = production::writePartDxfs(orderOf(state), target).size();
        })) {
        exported(state, std::string(tr(TextId::ExportDoneFiles)) + " " + std::to_string(count),
                 target, false);
    }
}

void exportCutListCsv(UiState& state)
{
    const auto path = showSaveDialog(FileKind::Csv, production::orderName(*state.document) + "_"
                                                        + documentWord(state, TextId::FileCutList) + ".csv");
    if (path && run(state, [&] {
            const i18n::LanguageScope inDocuments(documentLanguage(state));
            production::writeCutListCsv(orderOf(state), *path);
        })) {
        exported(state, tr(TextId::ExportDoneFile), *path, false);
    }
}

void exportPackage(UiState& state)
{
    const auto folder = showFolderDialog();
    if (!folder) {
        return;
    }
    if (nestingIsFresh(state)) {
        if (writePackageInto(state, *folder)) {
            state.openExportedPopup = true;
        }
        return;
    }
    // No plan for the product as it is now: compute it, the package follows (updateNesting).
    startNesting(state);
    if (state.nestingRun) {
        state.pendingPackage = *folder;
    }
}

bool writePackageInto(UiState& state, const std::filesystem::path& parent)
{
    const nesting::NestingResult* plan = freshPlan(state);
    const exporting::PdfFonts& fonts = pdfFonts(state);
    production::PackageOptions options;
    options.documentation = state.pdfOptions;
    std::optional<production::Package> package;
    if (!run(state, [&] {
            const i18n::LanguageScope inDocuments(documentLanguage(state));
            const auto folder =
                production::newPackageFolder(parent, production::orderName(*state.document));
            package = production::writePackage(orderOf(state), plan, state.cnc, options, fonts,
                                               folder);
        })) {
        return false;
    }
    // Said in the program's language, the package written in the documents'.
    state.exportMessage = std::string(tr(TextId::PackageDone)) + "\n"
                        + production::packageMessage(orderOf(state), plan, *package);
    state.exportedFile = package->folder;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------------------------

void drawCatalogWindow(UiState& state)
{
    if (ImGui::Begin(tr(TextId::WindowCatalog))) {
        ImGui::TextWrapped("%s", tr(TextId::CatalogHint));
        ImGui::Spacing();
        const float width = ImGui::GetContentRegionAvail().x;
        for (const core::CatalogItem item : core::catalogItems()) {
            ImGui::PushID(static_cast<int>(item));
            if (ImGui::Button(tr(catalogText(item)), ImVec2(width, 0.0f))) {
                confirmUnsaved(state, [&state, item] { newDocument(state, item); });
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
}

void drawParametersWindow(UiState& state)
{
    if (ImGui::Begin(tr(TextId::WindowParameters))) {
        ImGui::SeparatorText(tr(TextId::ProductHeader));
        if (drawProductRows(state)) {
            ImGui::End();
            return;
        }
        // A wardrobe: its carcass. A kitchen: its sizes; the cabinets are in the tree.
        core::Product& product = state.document->product();
        bool changed = false;
        if (core::Carcass* carcass = product.carcass()) {
            ImGui::SeparatorText(tr(TextId::Construction));
            changed = drawConstructionRows(state, *carcass);
        } else if (product.kitchen() != nullptr) {
            ImGui::SeparatorText(tr(TextId::KitchenSizes));
            changed = drawKitchenRows(state) || drawAddCabinet(state, std::nullopt, std::nullopt);
        }
        if (!changed) {
            ImGui::SeparatorText(tr(TextId::DecorHeader));
            changed = drawDecorRows(state);
        }
        if (!changed) {
            ImGui::SeparatorText(tr(TextId::ProfileHeader));
            drawProfileRows(state);
        }
    }
    ImGui::End();
}

void drawProductWindow(UiState& state)
{
    if (ImGui::Begin(tr(TextId::WindowProduct))) {
        const float total = ImGui::GetContentRegionAvail().y;
        constexpr float kSplitter = 6.0f;
        const float treeHeight = std::max(80.0f, (total - kSplitter) * state.treeFraction);

        if (ImGui::BeginChild("tree", ImVec2(0.0f, treeHeight), ImGuiChildFlags_Borders)) {
            drawTreeNode(state, state.document->product());
        }
        ImGui::EndChild();

        // Draggable splitter between the tree (top) and the properties (bottom).
        ImGui::InvisibleButton("splitter", ImVec2(-1.0f, kSplitter));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (ImGui::IsItemActive() && total > 0.0f) {
            state.treeFraction =
                std::clamp(state.treeFraction + ImGui::GetIO().MouseDelta.y / total, 0.15f, 0.85f);
        }

        if (ImGui::BeginChild("properties", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            drawProperties(state);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void drawViewportWindow(UiState& state, render::Renderer& renderer)
{
    if (ImGui::Begin(tr(TextId::WindowViewport))) {
        bool optionsChanged = ImGui::Checkbox(tr(TextId::ViewFronts), &state.sceneOptions.showFronts);
        ImGui::SameLine();
        optionsChanged |= ImGui::Checkbox(tr(TextId::ViewHoles), &state.sceneOptions.showHoles);
        ImGui::SameLine();
        if (ImGui::Button(tr(TextId::ViewFit))) {
            state.fitRequested = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", tr(TextId::ViewHint));
        if (optionsChanged) {
            state.sceneValid = false;
        }

        const ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x >= 8.0f && size.y >= 8.0f) {
            ensureScene(state);
            if (state.fitRequested) {
                state.camera.frame(state.scene.bounds);
                state.fitRequested = false;
            }
            const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
            const GLuint texture = renderer.render(state.scene, state.camera,
                                                   static_cast<int>(size.x * scale.x),
                                                   static_cast<int>(size.y * scale.y));
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Image(static_cast<ImTextureID>(texture), size, ImVec2(0, 1), ImVec2(1, 0));
            // An invisible button over the image owns the mouse, so dragging never moves
            // the window.
            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton("view", size,
                                   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
                                       | ImGuiButtonFlags_MouseButtonMiddle);
            handleViewportInput(state, origin, size);
        }
    }
    ImGui::End();
}

void drawCutListWindow(UiState& state)
{
    if (state.focusCutList > 0) {
        ImGui::SetNextWindowFocus();
        --state.focusCutList;
    }
    if (!ImGui::Begin(tr(TextId::WindowCutList))) {
        ImGui::End();
        return;
    }
    ed::Document& doc = *state.document;
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY
                                    | ImGuiTableFlags_SizingStretchProp;
    const float height = ImGui::GetContentRegionAvail().y * 0.68f;
    int total = 0;
    std::optional<core::ComponentId> clicked;
    /// A "Kesim" box clicked: the parts of that row and their new state, applied after the
    /// table (the change rebuilds the cut list this loop is reading).
    std::optional<std::pair<std::vector<core::ComponentId>, bool>> toggled;
    if (ImGui::BeginTable("cutlist", 8, flags, ImVec2(0.0f, height))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const TextId column : {TextId::ColPart, TextId::ColMaterial, TextId::ColQuantity,
                                    TextId::ColFinish, TextId::ColCut, TextId::ColEdges,
                                    TextId::ColGrain}) {
            ImGui::TableSetupColumn(tr(column));
        }
        ImGui::TableSetupColumn(tr(TextId::ColCuttingPlan), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < doc.cutList().size(); ++i) {
            const detailing::CutListRow& row = doc.cutList()[i];
            total += row.quantity;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string name = i18n::partName(row.kind, row.name, row.hand);
            ImGui::PushID(static_cast<int>(i));
            // Parts the workshop does not cut are greyed out.
            if (!row.inCuttingPlan) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            if (ImGui::Selectable(name.c_str(), false,
                                  ImGuiSelectableFlags_SpanAllColumns
                                      | ImGuiSelectableFlags_AllowOverlap)) {
                clicked = row.components.front();
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.material.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", row.quantity);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                sizeText(row.finishLength, row.finishWidth, row.thickness).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(sizeText(row.cut.length, row.cut.width).c_str());
            ImGui::TableNextColumn();
            std::string edges;
            for (const core::EdgeBand& band : row.edges) {
                edges += (edges.empty() ? "" : " ")
                       + (band.present() ? mm(band.thickness) : std::string(tr(TextId::NoEdge)));
            }
            ImGui::TextUnformatted(edges.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                tr(row.keepGrainDirection ? TextId::GrainKept : TextId::GrainFree));
            if (!row.inCuttingPlan) {
                ImGui::PopStyleColor();
            }
            ImGui::TableNextColumn();
            bool inPlan = row.inCuttingPlan;
            if (ImGui::Checkbox("##cut", &inPlan)) {
                toggled.emplace(row.components, inPlan);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tr(TextId::InCuttingPlan));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (toggled) {
        run(state, [&] { ed::setInCuttingPlan(doc, toggled->first, toggled->second); });
    } else if (clicked) {
        doc.select(*clicked);
        state.scrollTreeToSelection = true;
    }
    ImGui::Text("%s: %d", tr(TextId::TotalParts), total);

    ImGui::SeparatorText(tr(TextId::EdgeUsage));
    if (ImGui::BeginTable("edges", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(tr(TextId::ColEdgeBand));
        ImGui::TableSetupColumn(tr(TextId::ColLength));
        ImGui::TableSetupColumn(tr(TextId::ColPieces));
        ImGui::TableHeadersRow();
        for (const detailing::EdgeBandUsage& usage : doc.edgeUsage()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(usage.band.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", usage.totalLength / 1000.0);
            ImGui::TableNextColumn();
            ImGui::Text("%d", usage.pieces);
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText(tr(TextId::HardwareList));
    if (ImGui::BeginTable("hardware", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(tr(TextId::ColItem));
        ImGui::TableSetupColumn(tr(TextId::ColSpec));
        ImGui::TableSetupColumn(tr(TextId::ColQuantity));
        ImGui::TableHeadersRow();
        for (const detailing::HardwareItem& item : doc.hardware()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr(hardwareText(item.kind)));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.spec.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.quantity);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void drawWarningsWindow(UiState& state)
{
    if (ImGui::Begin(tr(TextId::WindowWarnings))) {
        ed::Document& doc = *state.document;
        const auto& warnings = doc.detailingReport().warnings;
        if (warnings.empty() && doc.overlaps().empty() && doc.longWorktops().empty()) {
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s", tr(TextId::NoWarnings));
        }
        // Parts in each other's space (a project of an older version): a click selects one.
        std::optional<core::ComponentId> clicked;
        for (std::size_t i = 0; i < doc.overlaps().size(); ++i) {
            const core::Overlap& o = doc.overlaps()[i];
            const std::string text = std::string(tr(TextId::OverlapWarning)) + ": "
                                   + displayName(*o.first) + " — " + displayName(*o.second);
            ImGui::PushID(static_cast<int>(i));
            ImGui::Bullet();
            if (ImGui::Selectable(text.c_str())) {
                clicked = o.first->id();
            }
            ImGui::PopID();
        }
        // Worktops longer than a slab: a click selects one (its joint goes on a base cabinet).
        for (const core::Worktop* worktop : doc.longWorktops()) {
            const std::string text = std::string(tr(TextId::LongWorktopWarning)) + ": "
                                   + displayName(*worktop) + " " + mm(worktop->box().size().x) + mmUnit();
            ImGui::PushID(worktop);
            ImGui::Bullet();
            if (ImGui::Selectable(text.c_str())) {
                clicked = worktop->id();
            }
            ImGui::PopID();
        }
        for (const detailing::DetailingWarning& warning : warnings) {
            ImGui::BulletText("%s", detailing::warningText(warning).c_str());
        }
        if (clicked) {
            doc.select(*clicked);
            state.scrollTreeToSelection = true;
        }
    }
    ImGui::End();
}

namespace {

void drawPdfPopup(UiState& state)
{
    if (state.openPdfPopup) {
        ImGui::OpenPopup(tr(TextId::PdfWindow));
        state.openPdfPopup = false;
    }
    bool write = false;
    if (ImGui::BeginPopupModal(tr(TextId::PdfWindow), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        exporting::DocumentationOptions& o = state.pdfOptions;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
        if (ImGui::BeginCombo(tr(TextId::PaperSize), exporting::toString(o.format.size).data())) {
            for (const exporting::PaperSize size :
                 {exporting::PaperSize::A4, exporting::PaperSize::A3, exporting::PaperSize::A2,
                  exporting::PaperSize::A1}) {
                if (ImGui::Selectable(exporting::toString(size).data(), o.format.size == size)) {
                    o.format.size = size;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextUnformatted(tr(TextId::PaperOrientation));
        ImGui::SameLine();
        if (ImGui::RadioButton(tr(TextId::Landscape),
                               o.format.orientation == exporting::Orientation::Landscape)) {
            o.format.orientation = exporting::Orientation::Landscape;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(tr(TextId::Portrait),
                               o.format.orientation == exporting::Orientation::Portrait)) {
            o.format.orientation = exporting::Orientation::Portrait;
        }

        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        const std::string current = state.documentLanguage
                                        ? std::string(i18n::languageName(*state.documentLanguage))
                                        : std::string(tr(TextId::SameAsProgram));
        if (ImGui::BeginCombo(tr(TextId::DocumentLanguage), current.c_str())) {
            if (ImGui::Selectable(tr(TextId::SameAsProgram), !state.documentLanguage)) {
                state.documentLanguage.reset();
                saveSettingsFile(state);
            }
            for (const i18n::Language language : i18n::languages()) {
                const std::string name(i18n::languageName(language));
                if (ImGui::Selectable(name.c_str(), state.documentLanguage == language)) {
                    state.documentLanguage = language;
                    saveSettingsFile(state);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SeparatorText(tr(TextId::ReportSections));
        ImGui::Checkbox(tr(TextId::PageSummary), &o.summary);
        ImGui::Checkbox(tr(TextId::PageCutList), &o.cutList);
        ImGui::Checkbox(tr(TextId::PageHardware), &o.hardware);
        ImGui::Checkbox(tr(TextId::PageAssembly), &o.assembly);
        ImGui::Checkbox(tr(TextId::PagePart), &o.partDrawings);
        const bool cutting = nestingIsFresh(state) && !state.nesting->layouts.empty();
        ImGui::BeginDisabled(!cutting);
        bool cuttingPlan = cutting && o.cuttingPlan;
        if (ImGui::Checkbox(tr(TextId::PageCutting), &cuttingPlan)) {
            o.cuttingPlan = cuttingPlan;
        }
        ImGui::EndDisabled();
        if (!cutting) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", tr(TextId::Calculate));
        }

        ImGui::SeparatorText(tr(TextId::TitleBlock));
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
        ImGui::InputText(tr(TextId::CompanyName), &o.company);
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
        ImGui::InputText(tr(TextId::DesignerName), &o.designer);
        ImGui::Spacing();
        ImGui::Checkbox(tr(TextId::OpenAfterExport), &state.openAfterExport);
        ImGui::Spacing();

        const bool any = o.summary || o.cutList || o.hardware || o.assembly || o.partDrawings
                      || (cutting && o.cuttingPlan);
        ImGui::BeginDisabled(!any);
        if (ImGui::Button(tr(TextId::ExportButton))) {
            write = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(tr(TextId::Cancel))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (write) {
        writePdf(state); // after EndPopup: the native dialog must not run inside the popup
    }
}

void drawExportedPopup(UiState& state)
{
    if (state.openExportedPopup) {
        ImGui::OpenPopup(tr(TextId::ExportDoneTitle));
        state.openExportedPopup = false;
    }
    if (!ImGui::BeginPopupModal(tr(TextId::ExportDoneTitle), nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextUnformatted(state.exportMessage.c_str());
    ImGui::TextUnformatted(toUtf8(state.exportedFile).c_str());
    ImGui::Spacing();
    std::error_code ec;
    const bool folder = std::filesystem::is_directory(state.exportedFile, ec);
    if (!folder) {
        if (ImGui::Button(tr(TextId::OpenFile))) {
            openWithSystem(state.exportedFile);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
    }
    if (ImGui::Button(tr(TextId::OpenFolder))) {
        openWithSystem(folder ? state.exportedFile : state.exportedFile.parent_path());
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(TextId::Close))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace

void drawPopups(UiState& state)
{
    drawPdfPopup(state);
    drawExportedPopup(state);

    if (state.openErrorPopup) {
        ImGui::OpenPopup(tr(TextId::ErrorTitle));
        state.openErrorPopup = false;
    }
    if (ImGui::BeginPopupModal(tr(TextId::ErrorTitle), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(tr(TextId::ErrorOperationFailed));
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(state.errorMessage.c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button(tr(TextId::Close))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (state.openUnsavedPopup) {
        ImGui::OpenPopup(tr(TextId::UnsavedTitle));
        state.openUnsavedPopup = false;
    }
    if (ImGui::BeginPopupModal(tr(TextId::UnsavedTitle), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(tr(TextId::UnsavedBody));
        ImGui::Spacing();
        std::function<void()> action;
        if (ImGui::Button(tr(TextId::Save))) {
            if (saveDocument(state, false)) {
                action = std::move(state.pendingAction);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(TextId::DontSave))) {
            action = std::move(state.pendingAction);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(TextId::Cancel))) {
            state.pendingAction = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        if (action) {
            action();
        }
    }
}

} // namespace rc::ui
