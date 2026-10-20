#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace rc::ui {

struct ApplicationConfig {
    int windowWidth = 1600;
    int windowHeight = 900;
    bool vsync = true;

    std::optional<std::filesystem::path> openFile; // project to open at start
    int catalogItem = 0;                           // otherwise: this catalogue model
    bool showHoles = false;
    bool showFronts = true;
    std::optional<std::string> selectKind; // select the first component of this kind, e.g. "Shelf"
    bool pdfDialog = false;                 // open the PDF export settings at start
    /// Stage 7: compute the cutting plan at start (0 panel saw, 1 nesting centre, -1 no).
    int nesting = -1;
    int productCount = 1;                   // copies of the product in the cutting plan
    /// The cutting plan without the remnants on stock, for this run (the settings file keeps
    /// its own choice).
    bool noRemnants = false;
    bool stockWindow = false;               // bring the stock window to the front
    bool cutListWindow = false;             // bring the cut list to the front
    bool cuttingSettings = false;           // open the cutting settings at start
    /// Folder of the stock and settings files instead of the user's own (demos, tests).
    std::optional<std::filesystem::path> dataFolder;
    /// Writes the PDF documents (default settings, with the cutting plan if one is computed)
    /// and exits — the documents without a click, e.g. for a demo or to check a layout.
    std::optional<std::filesystem::path> exportPdf;
    /// Writes the CNC programs of the cutting plan (needs `nesting` = 1) into this folder and
    /// exits.
    std::optional<std::filesystem::path> exportCnc;
    bool cncSettings = false;        // open the CNC settings at start
    bool toolpath = false;           // show the toolpath in the cutting plan window

    /// Renders a few frames, saves the window as a .bmp file and exits — for documentation and
    /// automated smoke tests of the GUI. A cutting plan started with `nesting` is finished first.
    std::optional<std::filesystem::path> screenshot;
};

/// Owns the main window, the OpenGL context and the ImGui context, and runs the frame loop.
///
/// Resource lifetime is strictly RAII (stage 10: no leaks): each resource is a separate member of
/// the private Impl, so if initialisation throws half-way, the already-created resources are
/// released in reverse order automatically. GLFW/ImGui types do not leak into this header.
class Application {
public:
    explicit Application(ApplicationConfig config = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /// Runs until the window is closed. Returns the process exit code.
    int run();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rc::ui
