#pragma once

#include "readycad/i18n/Localization.hpp"
#include "readycad/cam/NestingCenter.hpp"
#include "readycad/core/builders/KitchenBuilder.hpp"
#include "readycad/editor/Document.hpp"
#include "readycad/export/Documentation.hpp"
#include "readycad/nesting/NestingRun.hpp"
#include "readycad/nesting/Stock.hpp"
#include "readycad/production/Pipeline.hpp"
#include "readycad/render/Camera.hpp"
#include "readycad/render/Renderer.hpp"
#include "readycad/render/Scene.hpp"

#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace rc::ui {

/// Everything the windows share. The model lives in `document`; the UI keeps only view state.
struct UiState {
    std::unique_ptr<editor::Document> document;
    std::uint64_t documentSerial = 0; ///< increases whenever another document is opened

    // 3D view
    render::OrbitCamera camera;
    render::SceneOptions sceneOptions;
    bool fitRequested = true;
    render::Scene scene;
    std::uint64_t sceneRevision = 0;
    bool sceneValid = false;
    int dragButton = -1;
    bool dragMoved = false;
    ImVec2 dragStart{};

    // Selection made in the 3D view: the tree scrolls to it.
    bool scrollTreeToSelection = false;

    // Messages
    std::string errorMessage;
    bool openErrorPopup = false;

    // "Unsaved changes?" before New / Open / Exit
    std::function<void()> pendingAction;
    bool openUnsavedPopup = false;
    bool quit = false;

    float treeFraction = 0.42f; // share of the product window used by the tree

    // Export (stage 6)
    exporting::DocumentationOptions pdfOptions;
    /// The language of the documents and files the program writes; nullopt: the program's.
    std::optional<i18n::Language> documentLanguage;
    bool openAfterExport = true;
    bool openPdfPopup = false;
    std::optional<exporting::PdfFonts> pdfFonts; // looked up once
    std::string exportMessage;                   // "exported" popup
    std::filesystem::path exportedFile;
    bool openExportedPopup = false;

    // Cutting and nesting (stage 7). The stock and the settings belong to the workshop: they
    // live in files of the user folder, not in the project.
    nesting::StockManager stock;
    std::uint64_t stockRevision = 0; ///< increases with every change of the stock
    std::filesystem::path stockFile;
    std::filesystem::path settingsFile;
    nesting::NestingSettings nestingSettings;
    nesting::NestingSettings settingsDraft; ///< edited in the settings dialog
    bool openNestingSettings = false;
    int productCount = 1;
    std::unique_ptr<nesting::NestingRun> nestingRun; ///< running (or just finished) optimisation
    std::uint64_t nestingVersion = 0;                ///< of the run's result last shown
    std::optional<nesting::NestingResult> nesting;   ///< shown result; live while running
    std::uint64_t nestingChanges = 0;                ///< increases whenever `nesting` changes

    /// What the shown result was computed for: when any of it changes, the result is stale.
    struct NestingInput {
        std::uint64_t document = 0;
        std::uint64_t revision = 0;
        std::uint64_t stock = 0;
        int productCount = 1;
    } nestingFor;
    bool nestingConsumed = false; ///< the stock was already updated with this result
    std::size_t selectedSheet = 0;
    bool showSawCuts = true;
    bool openConsumePopup = false;
    std::string nestingMessage; ///< status line under the sheet list
    int focusNesting = 0; ///< frames left to bring the window to the front (docking settles first)
    int focusStock = 0;
    int focusCutList = 0;

    // "Add sheet" form of the stock window
    int newSheetMaterial = 0;
    nesting::Sheet newSheet;

    // CNC programs (stage 8): the nesting centre, part of the settings file
    cam::NestingCenterProfile cnc;
    cam::NestingCenterProfile cncDraft; ///< edited in the CNC settings dialog
    bool openCncSettings = false;
    bool showToolpath = false;
    /// Program of the sheet shown with its toolpath; rebuilt when the key changes.
    std::optional<cam::SheetProgram> toolpath;
    std::string toolpathError;
    struct ToolpathKey {
        std::uint64_t nesting = 0; ///< nestingChanges
        std::size_t sheet = 0;
        cam::NestingCenterProfile profile;
        friend bool operator==(const ToolpathKey&, const ToolpathKey&) = default;
    };
    std::optional<ToolpathKey> toolpathKey;

    // Production package (stage 9): asked for while the cutting plan was still being computed;
    // written into this folder as soon as the plan is ready.
    std::optional<std::filesystem::path> pendingPackage;

    // Kitchens: the "add a cabinet" form
    core::CabinetModule newCabinet = core::CabinetModule::BaseDoors;
    core::Real newCabinetWidth = 600.0;
    core::KitchenWall newCabinetWall = core::KitchenWall::Back;
};

/// Writes the cutting and CNC settings and the languages into the settings file.
void saveSettingsFile(UiState& state);

/// The language the documents are written in now (UiState::documentLanguage or the program's).
[[nodiscard]] i18n::Language documentLanguage(const UiState& state) noexcept;

/// Runs a model operation; on an exception shows the error popup. Returns true if it succeeded.
/// After calling it, a panel must not use references into the model taken before: a failed
/// operation rebuilds the model (strong exception guarantee of Document::edit).
bool run(UiState& state, const std::function<void()>& operation);

/// Runs `action` right away, or first asks to save if the document has unsaved changes.
void confirmUnsaved(UiState& state, std::function<void()> action);

/// Saves (asks for a file name if there is none yet). Returns false if cancelled or failed.
bool saveDocument(UiState& state, bool forceDialog);

/// Makes `document` the open one: the 3D view refits, cutting results of the old one go.
void setDocument(UiState& state, std::unique_ptr<editor::Document> document);
void newDocument(UiState& state, core::CatalogItem item);
void openDocument(UiState& state);
void loadProfile(UiState& state);
void saveProfile(UiState& state);

// Export: PDF documents (asks for the settings first), all parts in one DXF, one DXF per part.
void exportPdf(UiState& state);
/// Writes the PDF documents with the current settings to `path`, without dialogs (command line).
bool writePdfFile(UiState& state, const std::filesystem::path& path);
void exportPartsDxf(UiState& state);
void exportPartsDxfFolder(UiState& state);
void exportCutListCsv(UiState& state);

// Production package (stage 9): everything for the workshop in one folder, made by the same
// production pipeline as the command line and the tests.
/// Asks for a folder. With a fresh cutting plan the package is written right away; otherwise
/// the plan is computed first and the package follows when it is ready.
void exportPackage(UiState& state);
/// Writes the package of the current, fresh plan into a new sub-folder of `parent`, without
/// dialogs; prepares the "exported" message. Returns false on an error (shown in the popup).
bool writePackageInto(UiState& state, const std::filesystem::path& parent);

// Windows
void drawCatalogWindow(UiState& state);
void drawParametersWindow(UiState& state);
void drawProductWindow(UiState& state);
void drawViewportWindow(UiState& state, render::Renderer& renderer);
void drawCutListWindow(UiState& state);
void drawWarningsWindow(UiState& state);
void drawPopups(UiState& state);

// Cutting and nesting (stage 7), NestingPanels.cpp
/// Loads the stock and the cutting settings from `folder` (empty: the user folder, see
/// storage::userDataFolder()). Missing files: an empty stock and default settings.
void loadWorkshopFiles(UiState& state, const std::filesystem::path& folder = {});
void startNesting(UiState& state);
void stopNesting(UiState& state);
/// Asks for confirmation, then takes the sheets of the cutting plan out of the stock.
void consumeStock(UiState& state);
void exportSheetsDxf(UiState& state);
/// True if the shown cutting plan belongs to the current product, stock and settings.
[[nodiscard]] bool nestingIsFresh(const UiState& state);
/// Every frame, before the windows: takes over progress and results of the background run.
void updateNesting(UiState& state);
void drawNestingWindow(UiState& state);
void drawStockWindow(UiState& state);
void drawNestingPopups(UiState& state);

// CNC programs (stage 8), CncPanels.cpp
/// Asks for a folder and writes one program per sheet of a nesting centre plan.
void exportCncPrograms(UiState& state);
/// The same into `folder` without dialogs (command line); sets the "exported" message.
bool writeCncPrograms(UiState& state, const std::filesystem::path& folder);
/// The program of the selected sheet for the toolpath preview, planned again only when the
/// plan, the sheet or the machine changed. Null for a panel saw plan or if it cannot be made
/// (UiState::toolpathError says why).
const cam::SheetProgram* sheetToolpath(UiState& state);
/// "Tahmini süre: 4 dk 12 sn   Delik: 96   ..." and what is left for other machines.
[[nodiscard]] std::string toolpathSummary(const cam::SheetProgram& program);
void drawCncPopups(UiState& state);

} // namespace rc::ui
