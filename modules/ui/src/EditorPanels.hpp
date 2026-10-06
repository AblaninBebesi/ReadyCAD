#pragma once

#include "readycad/editor/Document.hpp"
#include "readycad/render/Camera.hpp"
#include "readycad/render/Renderer.hpp"
#include "readycad/render/Scene.hpp"

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace rc::ui {

/// Everything the windows share. The model lives in `document`; the UI keeps only view state.
struct UiState {
    std::unique_ptr<editor::Document> document;

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
};

/// Runs a model operation; on an exception shows the error popup. Returns true if it succeeded.
/// After calling it, a panel must not use references into the model taken before: a failed
/// operation rebuilds the model (strong exception guarantee of Document::edit).
bool run(UiState& state, const std::function<void()>& operation);

/// Runs `action` right away, or first asks to save if the document has unsaved changes.
void confirmUnsaved(UiState& state, std::function<void()> action);

/// Saves (asks for a file name if there is none yet). Returns false if cancelled or failed.
bool saveDocument(UiState& state, bool forceDialog);

void newDocument(UiState& state, core::CatalogItem item);
void openDocument(UiState& state);
void loadProfile(UiState& state);
void saveProfile(UiState& state);

// Windows
void drawCatalogWindow(UiState& state);
void drawParametersWindow(UiState& state);
void drawProductWindow(UiState& state);
void drawViewportWindow(UiState& state, render::Renderer& renderer);
void drawCutListWindow(UiState& state);
void drawWarningsWindow(UiState& state);
void drawPopups(UiState& state);

} // namespace rc::ui
