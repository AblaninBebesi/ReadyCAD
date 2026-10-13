#include "readycad/storage/Storage.hpp"

#include "Json.hpp"
#include "readycad/core/model/Drawer.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

using nlohmann::json;

namespace rc::storage {

using namespace rc::core;
using detail::checkHeader;
using detail::parse;

namespace {

/// Paths in messages: UTF-8 also for Turkish names (path::string() may throw on Windows).
std::string toUtf8(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

constexpr std::string_view kProjectFormat = "readycad-project";
constexpr std::string_view kProfileFormat = "readycad-profile";

// ---------------------------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------------------------

/// Per-part choices of a panel: name, material and edge overrides, left out of the cutting
/// plan. Only what differs from the defaults is written.
void writePanelChoices(const PanelComponent& part, json& out)
{
    if (!part.name().empty()) {
        out["name"] = part.name();
    }
    if (!part.inCuttingPlan()) {
        out["cutting"] = false;
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

/// Key of a drawer box part in the file; nullptr for the front (saved on its own).
const char* drawerPartKey(const FurnitureComponent& part)
{
    switch (part.kind()) {
    case ComponentKind::DrawerBoxSide:
        return static_cast<const DrawerBoxSide&>(part).side() == Side::Left ? "leftSide"
                                                                             : "rightSide";
    case ComponentKind::DrawerBoxEnd:
        return static_cast<const DrawerBoxEnd&>(part).end() == DrawerEnd::Front ? "frontEnd"
                                                                                : "backEnd";
    case ComponentKind::DrawerBottom:
        return "bottom";
    default:
        return nullptr;
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
            json item{{"front", front}};
            json box = json::object();
            for (const auto& part : drawer->children()) {
                if (const char* key = drawerPartKey(*part)) {
                    json choices = json::object();
                    writePanelChoices(static_cast<const PanelComponent&>(*part), choices);
                    if (!choices.empty()) {
                        box[key] = choices;
                    }
                }
            }
            if (!box.empty()) {
                item["box"] = box;
            }
            drawers.push_back(item);
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

/// The carcass's own parts, as it has them now, by their key in the file.
std::vector<std::pair<const char*, PanelComponent*>> namedParts(const Carcass& carcass)
{
    std::vector<std::pair<const char*, PanelComponent*>> named{
        {"leftSide", &carcass.leftSide()}, {"rightSide", &carcass.rightSide()}};
    if (carcass.topStyle() == CarcassTop::Panel) {
        named.emplace_back("top", &carcass.top());
    } else {
        named.emplace_back("frontRail", carcass.frontRail());
        named.emplace_back("rearRail", carcass.rearRail());
    }
    named.emplace_back("bottom", &carcass.bottom());
    named.emplace_back("back", &carcass.back());
    if (Plinth* plinth = carcass.plinth()) {
        named.emplace_back("plinth", plinth);
    }
    return named;
}

json writeCarcass(const Carcass& carcass)
{
    json parts;
    for (const auto& [key, part] : namedParts(carcass)) {
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
    out["topStyle"] = carcass.topStyle();
    if (carcass.hasPlinth()) {
        out["plinth"] = true;
    }
    if (carcass.fixedWidth()) {
        out["width"] = *carcass.fixedWidth();
    }
    if (carcass.cutout()) {
        out["cutout"] = *carcass.cutout();
    }
    if (!parts.empty()) {
        out["parts"] = parts;
    }
    out["interior"] = writeSection(carcass.interior());
    return out;
}

json writeKitchen(const Kitchen& kitchen)
{
    json out;
    out["sizes"] = kitchen.sizes();
    json worktop = json::object();
    writePanelChoices(kitchen.worktop(), worktop);
    out["worktop"] = worktop;
    for (const auto& [key, row] : {std::pair{"base", &kitchen.baseRow()},
                                   std::pair{"wall", &kitchen.wallRow()}}) {
        json cabinets = json::array();
        for (const Carcass* cabinet : row->cabinets()) {
            cabinets.push_back(writeCarcass(*cabinet));
        }
        out[key] = cabinets;
    }
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
    part.setInCuttingPlan(in.value("cutting", true));
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
            const json box = drawers.at(i).value("box", json::object());
            for (const auto& part : created[i]->children()) {
                const char* key = drawerPartKey(*part);
                if (key != nullptr && box.contains(key)) {
                    readPanelChoices(box.at(key), static_cast<PanelComponent&>(*part));
                }
            }
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
    carcass.setTopStyle(in.value("topStyle", CarcassTop::Panel));
    carcass.setPlinth(in.value("plinth", false));
    carcass.setFixedWidth(readOptionalReal(in, "width"));
    if (in.contains("cutout") && !in.at("cutout").is_null()) {
        carcass.setCutout(in.at("cutout").get<WorktopCutout>());
    }
    if (in.contains("parts")) {
        const json& parts = in.at("parts");
        for (const auto& [key, part] : namedParts(carcass)) {
            if (parts.contains(key)) {
                readPanelChoices(parts.at(key), *part);
            }
        }
    }
    readSection(in.at("interior"), carcass.interior());
}

void readKitchen(const json& in, Kitchen& kitchen)
{
    kitchen.setSizes(in.value("sizes", json::object()).get<Kitchen::Sizes>());
    // Always read: a worktop starts out of the cutting plan, so for it a missing "cutting" key
    // means "in the plan".
    readPanelChoices(in.value("worktop", json::object()), kitchen.worktop());
    for (const auto& [key, row] : {std::pair{"base", &kitchen.baseRow()},
                                   std::pair{"wall", &kitchen.wallRow()}}) {
        for (const json& cabinet : in.value(key, json::array())) {
            auto carcass = std::make_shared<Carcass>();
            row->add(carcass);
            readCarcass(cabinet, *carcass);
        }
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
    const Kitchen* kitchen = product.kitchen();
    if (carcass == nullptr && kitchen == nullptr) {
        throw StorageError("The product has neither a carcass nor a kitchen");
    }
    json doc;
    doc["format"] = std::string(kProjectFormat);
    doc["version"] = kProjectFormatVersion;
    doc["name"] = product.name();
    doc["dimensions"] = product.dimensions();
    doc["rules"] = product.rules();
    if (carcass != nullptr) {
        doc["carcass"] = writeCarcass(*carcass);
    } else {
        doc["kitchen"] = writeKitchen(*kitchen);
    }
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
        if (doc.contains("kitchen")) {
            auto kitchen = std::make_shared<Kitchen>();
            product->add(kitchen);
            readKitchen(doc.at("kitchen"), *kitchen);
        } else {
            auto carcass = std::make_shared<Carcass>();
            product->add(carcass);
            readCarcass(doc.at("carcass"), *carcass);
        }
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
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw StorageError("Cannot open the file for writing: " + toUtf8(path));
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.close();
        if (!out) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw StorageError("Cannot write the file: " + toUtf8(path));
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec); // replaces an existing file
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw StorageError("Cannot write the file: " + toUtf8(path) + " (" + ec.message() + ")");
    }
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw StorageError("Cannot open the file: " + toUtf8(path));
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace rc::storage
