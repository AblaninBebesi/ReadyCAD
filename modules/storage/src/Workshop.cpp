// Files of the workshop rather than of a project (stage 7): the stock of sheets and remnants,
// and the settings of the cutting machines and the optimiser.

#include "Json.hpp"
#include "readycad/storage/Storage.hpp"

#include <cstdlib>
#include <stdexcept>

using nlohmann::json;

namespace rc::nesting {

NLOHMANN_JSON_SERIALIZE_ENUM(Machine, {
    {Machine::PanelSaw, "panelSaw"},
    {Machine::NestingCenter, "nestingCenter"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(CutDirection, {
    {CutDirection::AlongLength, "alongLength"},
    {CutDirection::AlongWidth, "alongWidth"},
    {CutDirection::Auto, "auto"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PanelSawSettings, kerf, trim, maxStages,
                                                maxStackHeight, firstCuts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NestingCenterSettings, toolDiameter, extraSpacing,
                                                margin)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(OptimizerSettings, populationSize, generations,
                                                mutationRate, elites, threads, memoryLimitMb, seed)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NestingSettings, machine, saw, router, optimizer)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RemnantRules, minLength, minWidth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Sheet, id, material, length, width, quantity,
                                                remnant, note)

} // namespace rc::nesting

namespace rc::cam {

NLOHMANN_JSON_SERIALIZE_ENUM(ToolKind, {
    {ToolKind::Router, "router"},
    {ToolKind::Drill, "drill"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(ZeroPlane, {
    {ZeroPlane::Table, "table"},
    {ZeroPlane::Top, "top"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(CommentStyle, {
    {CommentStyle::Parentheses, "parentheses"},
    {CommentStyle::Semicolon, "semicolon"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Tool, number, kind, diameter, cuttingLength, rpm,
                                                feed, plungeFeed, stepDown, name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GCodeDialect, name, extension, decimals,
                                                lineNumbers, lineNumberStep, comments,
                                                percentLines, zero, header, footer, toolChange,
                                                spindleOn, drillCycles)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NestingCenterProfile, tools, contourTool,
                                                spoilboardCut, safeHeight, clearance,
                                                climbMilling, rapidSpeed, toolChangeSeconds,
                                                dialect)

} // namespace rc::cam

namespace rc::storage {

namespace {

constexpr std::string_view kStockFormat = "readycad-stock";
constexpr std::string_view kSettingsFormat = "readycad-settings";

} // namespace

std::string saveStock(const nesting::StockManager& stock)
{
    json doc;
    doc["format"] = std::string(kStockFormat);
    doc["version"] = kStockFormatVersion;
    doc["standardSheet"] = {{"length", stock.standardLength}, {"width", stock.standardWidth}};
    doc["remnantRules"] = stock.remnantRules;
    doc["sheets"] = stock.sheets();
    return doc.dump(2);
}

nesting::StockManager loadStock(std::string_view text)
{
    const json doc = detail::parse(text);
    detail::checkHeader(doc, kStockFormat, kStockFormatVersion);
    try {
        nesting::StockManager stock(
            doc.value("sheets", json::array()).get<std::vector<nesting::Sheet>>());
        const json standard = doc.value("standardSheet", json::object());
        stock.standardLength = standard.value("length", stock.standardLength);
        stock.standardWidth = standard.value("width", stock.standardWidth);
        stock.remnantRules =
            doc.value("remnantRules", json::object()).get<nesting::RemnantRules>();
        if (stock.standardLength <= 0.0 || stock.standardWidth <= 0.0) {
            throw std::invalid_argument("standard sheet size must be positive");
        }
        return stock;
    } catch (const json::exception& e) {
        throw StorageError(std::string("Invalid stock file: ") + e.what());
    } catch (const std::invalid_argument& e) {
        throw StorageError(std::string("Inconsistent stock file: ") + e.what());
    }
}

std::string saveSettings(const AppSettings& settings)
{
    json doc;
    doc["format"] = std::string(kSettingsFormat);
    doc["version"] = kSettingsFormatVersion;
    doc["nesting"] = settings.nesting;
    doc["cnc"] = settings.cnc;
    return doc.dump(2);
}

AppSettings loadSettings(std::string_view text)
{
    const json doc = detail::parse(text);
    detail::checkHeader(doc, kSettingsFormat, kSettingsFormatVersion);
    try {
        AppSettings settings;
        settings.nesting = doc.value("nesting", json::object()).get<nesting::NestingSettings>();
        settings.cnc = doc.value("cnc", json::object()).get<cam::NestingCenterProfile>();
        nesting::validate(settings.nesting);
        cam::validate(settings.cnc);
        return settings;
    } catch (const json::exception& e) {
        throw StorageError(std::string("Invalid settings file: ") + e.what());
    } catch (const std::invalid_argument& e) {
        throw StorageError(e.what());
    } catch (const cam::CamError& e) {
        throw StorageError(e.what());
    }
}

WorkshopFiles WorkshopFiles::in(const std::filesystem::path& folder)
{
    const std::filesystem::path base = folder.empty() ? userDataFolder() : folder;
    return {base / "stock.rcstock", base / "settings.json"};
}

namespace {

bool fileExists(const std::filesystem::path& file)
{
    std::error_code ec;
    return std::filesystem::exists(file, ec);
}

void createFolderOf(const std::filesystem::path& file)
{
    std::error_code ec;
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path(), ec);
    }
    // A failure shows up as a clear write error in writeTextFile().
}

} // namespace

nesting::StockManager loadStockFile(const std::filesystem::path& file)
{
    return fileExists(file) ? loadStock(readTextFile(file)) : nesting::StockManager{};
}

AppSettings loadSettingsFile(const std::filesystem::path& file)
{
    return fileExists(file) ? loadSettings(readTextFile(file)) : AppSettings{};
}

void saveStockFile(const nesting::StockManager& stock, const std::filesystem::path& file)
{
    createFolderOf(file);
    writeTextFile(file, saveStock(stock));
}

void saveSettingsFile(const AppSettings& settings, const std::filesystem::path& file)
{
    createFolderOf(file);
    writeTextFile(file, saveSettings(settings));
}

std::filesystem::path userDataFolder()
{
#ifdef _WIN32
    // The wide variant: a Turkish or Russian user name must not turn into question marks.
    if (const wchar_t* appData = _wgetenv(L"APPDATA"); appData != nullptr && *appData != 0) {
        return std::filesystem::path(appData) / "ReadyCAD";
    }
#else
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config != nullptr && *config != 0) {
        return std::filesystem::path(config) / "ReadyCAD";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != 0) {
        return std::filesystem::path(home) / ".config" / "ReadyCAD";
    }
#endif
    return std::filesystem::temp_directory_path() / "ReadyCAD";
}

} // namespace rc::storage
