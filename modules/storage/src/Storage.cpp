#include "readycad/storage/Storage.hpp"

#include "readycad/core/model/Drawer.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

using nlohmann::json;

// =============================================================================================
// JSON mapping of the plain data types. The macros generate to_json/from_json; they must live
// in the types' namespace so that nlohmann finds them (argument-dependent lookup).
// "_WITH_DEFAULT": a field missing in the file keeps its default value.
// =============================================================================================

namespace rc::core {

NLOHMANN_JSON_SERIALIZE_ENUM(MaterialType, {
    {MaterialType::Chipboard, "chipboard"},
    {MaterialType::MDF, "mdf"},
    {MaterialType::HDF, "hdf"},
    {MaterialType::Plywood, "plywood"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HandleType, {
    {HandleType::Bar, "bar"},
    {HandleType::Knob, "knob"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(ConstructionRules::JointFastener, {
    {ConstructionRules::JointFastener::Confirmat, "confirmat"},
    {ConstructionRules::JointFastener::Eccentric, "eccentric"},
    {ConstructionRules::JointFastener::Dowel, "dowel"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(JointType, {
    {JointType::Overlay, "overlay"},
    {JointType::Inset, "inset"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(BackMountType, {
    {BackMountType::Groove, "groove"},
    {BackMountType::Overlay, "overlay"},
    {BackMountType::Inset, "inset"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(ShelfMount, {
    {ShelfMount::Supports, "supports"},
    {ShelfMount::Confirmat, "confirmat"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HingeSide, {
    {HingeSide::Left, "left"},
    {HingeSide::Right, "right"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HandleOrientation, {
    {HandleOrientation::Vertical, "vertical"},
    {HandleOrientation::Horizontal, "horizontal"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(HandleAnchor, {
    {HandleAnchor::Top, "top"},
    {HandleAnchor::Center, "center"},
    {HandleAnchor::Bottom, "bottom"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(Axis, {
    {Axis::X, "x"},
    {Axis::Y, "y"},
    {Axis::Z, "z"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Material, name, type, thickness, hasGrain)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EdgeBand, thickness, name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Size3D, width, height, depth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HandleModel, id, name, supplier, article, type,
                                                holeSpacing, holeDiameter, length, width,
                                                projection)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HandlePlacement, modelId, orientation, anchor,
                                                edgeOffset, anchorOffset)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::System32, pitch,
                                                frontRowOffset, rearRowMinOffset, snapShelves)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Joints, fastener,
                                                alignmentDowels, endOffset, maxSpacing)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Confirmat, faceDiameter,
                                                edgeDiameter, length)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Dowel, diameter, length,
                                                faceDepth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Eccentric, camDiameter,
                                                camDepth, camDistance, channelDiameter,
                                                boltHoleDiameter, boltHoleDepth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::ShelfSupport, diameter, depth,
                                                holeBelowShelf, extraHoles)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Hinge, cupDiameter, cupDepth,
                                                cupEdgeDistance, endOffset, plateHoleDiameter,
                                                plateHoleDepth, plateHoleSpacing, heightLimits)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Handles, catalog,
                                                defaultDoorHandle, defaultDrawerHandle,
                                                drillOnMachine, minEdgeDistance)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConstructionRules::Cutting, premillAllowance,
                                                rounding, edgeTrimAllowance)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ConstructionRules, carcassMaterial, backMaterial, frontMaterial, drawerBoxMaterial,
    drawerBottomMaterial, visibleEdge, hiddenEdge, backGrooveDepth, backGrooveOffset,
    backGrooveWidth, backExpansionGap, backOverlayInset, shelfSideGap, shelfFrontSetback,
    shelfBackGap, confirmatShelfFrontSetback, confirmatShelfBackGap, partitionFrontSetback,
    frontEdgeGap, frontGap, drawerSlideClearance, drawerBoxBottomClearance, drawerBoxTopClearance,
    drawerBoxMaxHeight, drawerRearClearance, drawerSlideLengths, system32, joints, confirmat,
    dowel, eccentric, shelfSupport, hinge, minWallBetweenHoles, handles, cutting)

} // namespace rc::core

namespace rc::storage {

using namespace rc::core;

namespace {

constexpr std::string_view kProjectFormat = "readycad-project";
constexpr std::string_view kProfileFormat = "readycad-profile";

// ---------------------------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------------------------

/// Per-part choices of a panel: name, material and edge overrides. Only what differs from
/// the defaults is written.
void writePanelChoices(const PanelComponent& part, json& out)
{
    if (!part.name().empty()) {
        out["name"] = part.name();
    }
    if (part.materialOverride()) {
        out["material"] = *part.materialOverride();
    }
    json edges = json::object();
    const std::pair<EdgeSide, const char*> sides[] = {
        {EdgeSide::L1, "L1"}, {EdgeSide::L2, "L2"}, {EdgeSide::W1, "W1"}, {EdgeSide::W2, "W2"}};
    for (const auto& [side, key] : sides) {
        if (const auto& band = part.edgeOverride(side)) {
            edges[key] = *band;
        }
    }
    if (!edges.empty()) {
        out["edges"] = edges;
    }
}

json writeHandle(const FrontPanel& front)
{
    return front.handle() ? json(*front.handle()) : json(nullptr);
}

json writeSection(const Section& section);

json writeChild(const FurnitureComponent& child)
{
    json out;
    switch (child.kind()) {
    case ComponentKind::Section:
        out = writeSection(static_cast<const Section&>(child));
        out["type"] = "section";
        return out;
    case ComponentKind::Partition:
        out["type"] = "partition";
        break;
    case ComponentKind::FixedShelf:
        out["type"] = "fixedShelf";
        break;
    case ComponentKind::Shelf: {
        const auto& shelf = static_cast<const Shelf&>(child);
        out["type"] = "shelf";
        out["mount"] = shelf.mount();
        out["elevation"] = shelf.elevation() ? json(*shelf.elevation()) : json(nullptr);
        break;
    }
    case ComponentKind::Door: {
        const auto& door = static_cast<const Door&>(child);
        out["type"] = "door";
        out["hinge"] = door.hinge();
        out["handle"] = writeHandle(door);
        break;
    }
    case ComponentKind::DrawerStack: {
        out["type"] = "drawers";
        json drawers = json::array();
        for (const Drawer* drawer : static_cast<const DrawerStack&>(child).drawers()) {
            json front;
            front["handle"] = writeHandle(drawer->front());
            writePanelChoices(drawer->front(), front);
            drawers.push_back({{"front", front}});
        }
        out["drawers"] = drawers;
        return out;
    }
    default:
        throw StorageError("Cannot save a " + std::string(toString(child.kind()))
                           + " inside a section");
    }
    writePanelChoices(static_cast<const PanelComponent&>(child), out);
    return out;
}

json writeSection(const Section& section)
{
    json out;
    if (!section.name().empty()) {
        out["name"] = section.name();
    }
    out["fixedSize"] = section.fixedSize() ? json(*section.fixedSize()) : json(nullptr);
    out["splitAxis"] = section.splitAxis();
    json children = json::array();
    for (const auto& child : section.children()) {
        children.push_back(writeChild(*child));
    }
    out["children"] = children;
    return out;
}

json writeCarcass(const Carcass& carcass)
{
    json parts;
    const std::pair<const char*, const PanelComponent*> named[] = {
        {"leftSide", &carcass.leftSide()}, {"rightSide", &carcass.rightSide()},
        {"top", &carcass.top()},           {"bottom", &carcass.bottom()},
        {"back", &carcass.back()},
    };
    for (const auto& [key, part] : named) {
        json choices = json::object();
        writePanelChoices(*part, choices);
        if (!choices.empty()) {
            parts[key] = choices;
        }
    }
    json out;
    out["topJoint"] = carcass.topJoint();
    out["bottomJoint"] = carcass.bottomJoint();
    out["backMount"] = carcass.backMountType();
    if (!parts.empty()) {
        out["parts"] = parts;
    }
    out["interior"] = writeSection(carcass.interior());
    return out;
}

// ---------------------------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------------------------

void readPanelChoices(const json& in, PanelComponent& part)
{
    if (in.contains("name")) {
        part.setName(in.at("name").get<std::string>());
    }
    if (in.contains("material")) {
        part.setMaterialOverride(in.at("material").get<Material>());
    }
    if (in.contains("edges")) {
        const json& edges = in.at("edges");
        const std::pair<EdgeSide, const char*> sides[] = {
            {EdgeSide::L1, "L1"}, {EdgeSide::L2, "L2"}, {EdgeSide::W1, "W1"}, {EdgeSide::W2, "W2"}};
        for (const auto& [side, key] : sides) {
            if (edges.contains(key)) {
                part.setEdgeOverride(side, edges.at(key).get<EdgeBand>());
            }
        }
    }
}

std::optional<HandlePlacement> readHandle(const json& in)
{
    if (!in.contains("handle") || in.at("handle").is_null()) {
        return std::nullopt;
    }
    return in.at("handle").get<HandlePlacement>();
}

std::optional<Real> readOptionalReal(const json& in, const char* key)
{
    if (!in.contains(key) || in.at(key).is_null()) {
        return std::nullopt;
    }
    return in.at(key).get<Real>();
}

void readSection(const json& in, Section& section);

void readChild(const json& in, Section& parent)
{
    const std::string type = in.at("type").get<std::string>();
    if (type == "section") {
        auto section = std::make_shared<Section>();
        parent.add(section);
        readSection(in, *section);
    } else if (type == "partition") {
        auto partition = std::make_shared<Partition>();
        readPanelChoices(in, *partition);
        parent.add(partition);
    } else if (type == "fixedShelf") {
        auto shelf = std::make_shared<FixedShelf>();
        readPanelChoices(in, *shelf);
        parent.add(shelf);
    } else if (type == "shelf") {
        auto shelf = std::make_shared<Shelf>(readOptionalReal(in, "elevation"),
                                             in.value("mount", ShelfMount::Supports));
        readPanelChoices(in, *shelf);
        parent.add(shelf);
    } else if (type == "door") {
        auto door = std::make_shared<Door>(in.value("hinge", HingeSide::Left));
        door->setHandle(readHandle(in));
        readPanelChoices(in, *door);
        parent.add(door);
    } else if (type == "drawers") {
        const json& drawers = in.at("drawers");
        auto stack = std::make_shared<DrawerStack>(static_cast<int>(drawers.size()));
        const auto created = stack->drawers();
        for (std::size_t i = 0; i < created.size(); ++i) {
            const json& front = drawers.at(i).at("front");
            created[i]->front().setHandle(readHandle(front));
            readPanelChoices(front, created[i]->front());
        }
        parent.add(stack);
    } else {
        throw StorageError("Unknown section element type '" + type + "'");
    }
}

void readSection(const json& in, Section& section)
{
    if (in.contains("name")) {
        section.setName(in.at("name").get<std::string>());
    }
    section.setFixedSize(readOptionalReal(in, "fixedSize"));
    section.setSplitAxis(in.value("splitAxis", Axis::X));
    for (const json& child : in.value("children", json::array())) {
        readChild(child, section);
    }
}

void readCarcass(const json& in, Carcass& carcass)
{
    carcass.setTopJoint(in.value("topJoint", JointType::Overlay));
    carcass.setBottomJoint(in.value("bottomJoint", JointType::Inset));
    carcass.setBackMount(in.value("backMount", BackMountType::Groove));
    if (in.contains("parts")) {
        const json& parts = in.at("parts");
        const std::pair<const char*, PanelComponent*> named[] = {
            {"leftSide", &carcass.leftSide()}, {"rightSide", &carcass.rightSide()},
            {"top", &carcass.top()},           {"bottom", &carcass.bottom()},
            {"back", &carcass.back()},
        };
        for (const auto& [key, part] : named) {
            if (parts.contains(key)) {
                readPanelChoices(parts.at(key), *part);
            }
        }
    }
    readSection(in.at("interior"), carcass.interior());
}

json parse(std::string_view text)
{
    try {
        return json::parse(text);
    } catch (const json::exception& e) {
        throw StorageError(std::string("Invalid JSON: ") + e.what());
    }
}

void checkHeader(const json& doc, std::string_view format, int supportedVersion)
{
    if (!doc.is_object() || doc.value("format", std::string{}) != format) {
        throw StorageError("Not a " + std::string(format) + " file");
    }
    const int version = doc.value("version", 0);
    if (version < 1 || version > supportedVersion) {
        throw StorageError("Unsupported file version " + std::to_string(version));
    }
}

} // namespace

std::string saveProfile(const ConstructionRules& rules)
{
    json doc;
    doc["format"] = std::string(kProfileFormat);
    doc["version"] = kProfileFormatVersion;
    doc["rules"] = rules;
    return doc.dump(2);
}

ConstructionRules loadProfile(std::string_view text)
{
    const json doc = parse(text);
    checkHeader(doc, kProfileFormat, kProfileFormatVersion);
    try {
        return doc.value("rules", json::object()).get<ConstructionRules>();
    } catch (const json::exception& e) {
        throw StorageError(std::string("Invalid workshop profile: ") + e.what());
    }
}

std::string saveProduct(const Product& product)
{
    const Carcass* carcass = product.carcass();
    if (carcass == nullptr) {
        throw StorageError("The product has no carcass");
    }
    json doc;
    doc["format"] = std::string(kProjectFormat);
    doc["version"] = kProjectFormatVersion;
    doc["name"] = product.name();
    doc["dimensions"] = product.dimensions();
    doc["rules"] = product.rules();
    doc["carcass"] = writeCarcass(*carcass);
    return doc.dump(2);
}

std::shared_ptr<Product> loadProduct(std::string_view text)
{
    const json doc = parse(text);
    checkHeader(doc, kProjectFormat, kProjectFormatVersion);
    try {
        auto product = std::make_shared<Product>(doc.value("name", std::string{}),
                                                 doc.value("rules", json::object())
                                                     .get<ConstructionRules>());
        auto carcass = std::make_shared<Carcass>();
        product->add(carcass);
        readCarcass(doc.at("carcass"), *carcass);
        product->setDimensions(doc.at("dimensions").get<Size3D>()); // first layout
        return product;
    } catch (const json::exception& e) {
        throw StorageError(std::string("Invalid project file: ") + e.what());
    } catch (const std::invalid_argument& e) {
        throw StorageError(std::string("Inconsistent project file: ") + e.what());
    } catch (const std::logic_error& e) {
        throw StorageError(std::string("Inconsistent project file: ") + e.what());
    }
}

void writeTextFile(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw StorageError("Cannot open the file for writing: " + path.string());
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
        throw StorageError("Cannot write the file: " + path.string());
    }
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw StorageError("Cannot open the file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace rc::storage
