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

std::string saveNestingSettings(const nesting::NestingSettings& settings)
{
    json doc;
    doc["format"] = std::string(kSettingsFormat);
    doc["version"] = kSettingsFormatVersion;
    doc["nesting"] = settings;
    return doc.dump(2);
}

nesting::NestingSettings loadNestingSettings(std::string_view text)
{
    const json doc = detail::parse(text);
    detail::checkHeader(doc, kSettingsFormat, kSettingsFormatVersion);
    try {
        const auto settings = doc.value("nesting", json::object()).get<nesting::NestingSettings>();
        nesting::validate(settings);
        return settings;
    } catch (const json::exception& e) {
        throw StorageError(std::string("Invalid settings file: ") + e.what());
    } catch (const std::invalid_argument& e) {
        throw StorageError(e.what());
    }
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
