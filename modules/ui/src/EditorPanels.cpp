#include "EditorPanels.hpp"

#include "FileDialogs.hpp"
#include "readycad/core/model/Drawer.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/editor/PartInfo.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/storage/Storage.hpp"
#include "readycad/i18n/Labels.hpp"

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
        ImGui::TextUnformatted(label);
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
    return ImGui::DragScalar(id, ImGuiDataType_Double, &value, 1.0f, &min, &max, "%.0f mm",
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
    const std::pair<TextId, Real*> dims[] = {
        {TextId::Width, &d.width}, {TextId::Height, &d.height}, {TextId::Depth, &d.depth}};
    const std::pair<Real, Real> limits[] = {{300.0, 3000.0}, {300.0, 3000.0}, {200.0, 1000.0}};
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

bool drawConstructionRows(UiState& s)
{
    ed::Document& doc = *s.document;
    core::Carcass* carcass = doc.product().carcass();
    if (carcass == nullptr) {
        return false;
    }
    PropertyTable table("construction");
    if (!table.isOpen()) {
        return false;
    }

    core::JointType top = carcass->topJoint();
    PropertyTable::row(tr(TextId::TopJoint));
    if (enumCombo("##top", top, kJoints)) {
        run(s, [&] { ed::setTopJoint(doc, top); });
        return true;
    }
    core::JointType bottom = carcass->bottomJoint();
    PropertyTable::row(tr(TextId::BottomJoint));
    if (enumCombo("##bottom", bottom, kJoints)) {
        run(s, [&] { ed::setBottomJoint(doc, bottom); });
        return true;
    }
    core::BackMountType back = carcass->backMountType();
    PropertyTable::row(tr(TextId::BackMount));
    if (enumCombo("##back", back, kBackMounts)) {
        run(s, [&] { ed::setBackMount(doc, back); });
        return true;
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

/// Kind-specific parameters of the selected component.
bool drawEditor(UiState& s, core::FurnitureComponent& c)
{
    switch (c.kind()) {
    case ComponentKind::Product:
        return drawProductRows(s);
    case ComponentKind::Carcass:
        return drawConstructionRows(s);
    case ComponentKind::Section:
        return drawSectionEditor(s, static_cast<core::Section&>(c));
    case ComponentKind::Shelf:
        return drawShelfEditor(s, static_cast<core::Shelf&>(c));
    case ComponentKind::Door:
        return drawDoorEditor(s, static_cast<core::Door&>(c));
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
                           sizeText(info.size.width, info.size.height, info.size.depth) + " mm");
    if (!info.panel) {
        return;
    }
    const ed::PanelInfo& p = *info.panel;
    PropertyTable::textRow(tr(TextId::Material), p.material.name);
    PropertyTable::textRow(tr(TextId::Thickness), mm(p.thickness) + " mm");
    PropertyTable::textRow(tr(TextId::FinishSize), sizeText(p.finishLength, p.finishWidth) + " mm");
    PropertyTable::textRow(tr(TextId::CutSize), sizeText(p.cut.length, p.cut.width) + " mm");

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
    const bool openByDefault = c.kind() == ComponentKind::Product
                            || c.kind() == ComponentKind::Carcass
                            || c.kind() == ComponentKind::Section;
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

/// Name for exported files: the project file's name, else the product's.
std::string exportBaseName(const ed::Document& doc)
{
    if (!doc.path().empty()) {
        return toUtf8(doc.path().stem());
    }
    return doc.product().name().empty() ? tr(TextId::Untitled) : doc.product().name();
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

void writePdf(UiState& state)
{
    const auto path = showSaveDialog(FileKind::Pdf, exportBaseName(*state.document) + ".pdf");
    if (path && writePdfFile(state, *path)) {
        exported(state, tr(TextId::ExportDoneFile), *path, state.openAfterExport);
    }
}

} // namespace

bool writePdfFile(UiState& state, const std::filesystem::path& path)
{
    const ed::Document& doc = *state.document;
    if (!state.pdfFonts) {
        state.pdfFonts = exporting::findPdfFonts();
    }
    exporting::DocumentationOptions options = state.pdfOptions;
    if (doc.product().name().empty() && !doc.path().empty()) {
        options.productName = toUtf8(doc.path().stem());
    }
    exporting::DocumentationInput input{doc.product(), doc.cutList(), doc.edgeUsage(),
                                        doc.detailingReport()};
    if (nestingIsFresh(state)) {
        input.cutting = &*state.nesting; // only a plan that belongs to this product as it is now
    }
    return run(state, [&] { exporting::saveDocumentationPdf(input, options, path, *state.pdfFonts); });
}

void exportPdf(UiState& state)
{
    state.openPdfPopup = true; // settings first, see drawPopups()
}

void exportPartsDxf(UiState& state)
{
    const ed::Document& doc = *state.document;
    const auto path = showSaveDialog(FileKind::Dxf, exportBaseName(doc) + ".dxf");
    if (!path) {
        return;
    }
    if (run(state, [&] {
            exporting::saveDxf(exporting::drawAllParts(doc.product(), doc.cutList()), *path);
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
    const ed::Document& doc = *state.document;
    // A folder of its own, ASCII-named like the files: CNC software and USB sticks cope with it.
    const std::filesystem::path target =
        *folder / (exporting::asciiFold(exportBaseName(doc)) + "_DXF");
    std::size_t count = 0;
    if (run(state, [&] {
            count = exporting::savePartDxfs(doc.product(), doc.cutList(), target).size();
        })) {
        exported(state, std::string(tr(TextId::ExportDoneFiles)) + " " + std::to_string(count),
                 target, false);
    }
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
        if (!drawProductRows(state)) {
            ImGui::SeparatorText(tr(TextId::Construction));
            if (!drawConstructionRows(state)) {
                ImGui::SeparatorText(tr(TextId::ProfileHeader));
                drawProfileRows(state);
            }
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
    if (ImGui::BeginTable("cutlist", 7, flags, ImVec2(0.0f, height))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const TextId column : {TextId::ColPart, TextId::ColMaterial, TextId::ColQuantity,
                                    TextId::ColFinish, TextId::ColCut, TextId::ColEdges,
                                    TextId::ColGrain}) {
            ImGui::TableSetupColumn(tr(column));
        }
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < doc.cutList().size(); ++i) {
            const detailing::CutListRow& row = doc.cutList()[i];
            total += row.quantity;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string name = row.name.empty() ? tr(kindText(row.kind)) : row.name;
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                clicked = row.components.front();
            }
            ImGui::PopID();
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
        }
        ImGui::EndTable();
    }
    if (clicked) {
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
        const auto& warnings = state.document->detailingReport().warnings;
        if (warnings.empty()) {
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s", tr(TextId::NoWarnings));
        }
        for (const std::string& warning : warnings) {
            ImGui::BulletText("%s", warning.c_str());
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
