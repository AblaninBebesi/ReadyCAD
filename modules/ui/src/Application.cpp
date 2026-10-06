#include "readycad/ui/Application.hpp"

#include "EditorPanels.hpp"
#include "FileDialogs.hpp"
#include "Fonts.hpp"
#include "readycad/core/Version.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/ui/Labels.hpp"
#include "readycad/ui/Localization.hpp"

#include <glad/gl.h> // must come before GLFW
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h> // DockBuilder API for the default window layout

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc::ui {

namespace {

void glfwErrorCallback(int code, const char* description)
{
    std::fprintf(stderr, "[GLFW] error %d: %s\n", code, description);
}

std::string glString(GLenum name)
{
    const auto* value = reinterpret_cast<const char*>(glGetString(name));
    return value != nullptr ? value : "?";
}

/// glfwInit / glfwTerminate.
struct GlfwLibrary {
    GlfwLibrary()
    {
        glfwSetErrorCallback(glfwErrorCallback);
        if (glfwInit() == GLFW_FALSE) {
            throw std::runtime_error("GLFW initialisation failed");
        }
    }
    ~GlfwLibrary() { glfwTerminate(); }
    GlfwLibrary(const GlfwLibrary&) = delete;
    GlfwLibrary& operator=(const GlfwLibrary&) = delete;
};

/// Main window with a current OpenGL 3.3 core context and loaded GL functions.
struct GlWindow {
    GLFWwindow* handle = nullptr;

    explicit GlWindow(const ApplicationConfig& config)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // required on macOS

        handle = glfwCreateWindow(config.windowWidth, config.windowHeight,
                                  tr(TextId::AppTitle), nullptr, nullptr);
        if (handle == nullptr) {
            throw std::runtime_error("Could not create an OpenGL 3.3 window (driver too old?)");
        }
        glfwMakeContextCurrent(handle);
        glfwSwapInterval(config.vsync ? 1 : 0);

        if (gladLoadGL(glfwGetProcAddress) == 0) {
            glfwDestroyWindow(handle); // destructor will not run: we are still in the constructor
            throw std::runtime_error("Could not load OpenGL functions (glad)");
        }
    }
    ~GlWindow() { glfwDestroyWindow(handle); }
    GlWindow(const GlWindow&) = delete;
    GlWindow& operator=(const GlWindow&) = delete;
};

/// ImGui context + GLFW/OpenGL3 backends.
struct ImGuiLayer {
    explicit ImGuiLayer(GLFWwindow* window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        io.IniFilename = "readycad_layout.ini";

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        float xScale = 1.0f;
        float yScale = 1.0f;
        glfwGetWindowContentScale(window, &xScale, &yScale);
        style.ScaleAllSizes(xScale);
        loadUiFonts(17.0f * xScale);

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");
    }
    ~ImGuiLayer()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
};

/// Default window arrangement: catalogue + parameters on the left, the 3D view in the middle,
/// product tree + properties on the right, cut list and warnings at the bottom.
void buildDefaultLayout(ImGuiID dockspace)
{
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);

    ImGuiID center = dockspace;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.30f, nullptr, &center);
    const ImGuiID bottom =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
    const ImGuiID leftBottom =
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.62f, nullptr, &left);

    ImGui::DockBuilderDockWindow(tr(TextId::WindowCatalog), left);
    ImGui::DockBuilderDockWindow(tr(TextId::WindowParameters), leftBottom);
    ImGui::DockBuilderDockWindow(tr(TextId::WindowProduct), right);
    ImGui::DockBuilderDockWindow(tr(TextId::WindowViewport), center);
    ImGui::DockBuilderDockWindow(tr(TextId::WindowCutList), bottom);
    ImGui::DockBuilderDockWindow(tr(TextId::WindowWarnings), bottom);
    ImGui::DockBuilderFinish(dockspace);
}

/// Writes the current back buffer as an uncompressed 24-bit BMP (no image library needed).
void writeBackBufferBmp(const std::filesystem::path& path, int width, int height)
{
    const int rowSize = (width * 3 + 3) & ~3; // BMP rows are padded to 4 bytes, like GL_PACK_ALIGNMENT 4
    std::vector<unsigned char> pixels(static_cast<std::size_t>(rowSize) * static_cast<std::size_t>(height));
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());

    auto put32 = [](unsigned char* p, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            p[i] = static_cast<unsigned char>(v >> (8 * i));
        }
    };
    unsigned char header[54] = {'B', 'M'};
    put32(header + 2, static_cast<std::uint32_t>(54 + pixels.size()));
    put32(header + 10, 54);
    put32(header + 14, 40);
    put32(header + 18, static_cast<std::uint32_t>(width));
    put32(header + 22, static_cast<std::uint32_t>(height)); // positive: bottom-up, as OpenGL
    header[26] = 1;                                         // planes
    header[28] = 24;                                        // bits per pixel
    put32(header + 34, static_cast<std::uint32_t>(pixels.size()));

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(header), sizeof header);
    out.write(reinterpret_cast<const char*>(pixels.data()),
              static_cast<std::streamsize>(pixels.size()));
}

} // namespace

struct Application::Impl {
    // Declaration order == construction order; destruction runs in reverse.
    ApplicationConfig config;
    GlfwLibrary glfw;
    GlWindow window;
    ImGuiLayer imgui;
    FileDialogSession dialogs;
    render::Renderer renderer; // GPU objects: destroyed before the context
    UiState state;

    std::string glRendererInfo;
    std::string windowTitle;
    bool showAbout = false;
    bool showImGuiDemo = false;
    bool layoutChecked = false;
    bool resetLayout = false;
    int frame = 0;

    explicit Impl(const ApplicationConfig& cfg)
        : config(cfg)
        , window(config)
        , imgui(window.handle)
        , glRendererInfo(glString(GL_RENDERER) + " | OpenGL " + glString(GL_VERSION))
    {
        state.sceneOptions.showHoles = config.showHoles;
        state.sceneOptions.showFronts = config.showFronts;
        const auto items = core::catalogItems();
        const auto index = static_cast<std::size_t>(
            std::clamp(config.catalogItem, 0, static_cast<int>(items.size()) - 1));
        newDocument(state, items[index]);
        if (config.openFile) {
            ui::run(state, [&] {
                state.document =
                    std::make_unique<editor::Document>(editor::Document::open(*config.openFile));
            });
        }
        if (config.selectKind) {
            state.document->product().visit([&](core::FurnitureComponent& c) {
                if (state.document->selected() == nullptr && core::toString(c.kind()) == *config.selectKind) {
                    state.document->select(c.id());
                    state.scrollTreeToSelection = true;
                }
            });
        }
    }

    void drawFrame();
    void drawMainMenu();
    void handleShortcuts();
    void drawAboutWindow();
    void updateWindowTitle();
    void requestQuit() { confirmUnsaved(state, [this] { state.quit = true; }); }
};

Application::Application(ApplicationConfig config) : impl_(std::make_unique<Impl>(config)) {}

Application::~Application() = default;

int Application::run()
{
    GLFWwindow* window = impl_->window.handle;
    while (!impl_->state.quit) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window) != GLFW_FALSE) {
            glfwSetWindowShouldClose(window, GLFW_FALSE); // ask about unsaved changes first
            impl_->requestQuit();
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            glfwWaitEvents(); // minimised: do not burn the GPU
            continue;
        }
        impl_->drawFrame();
    }
    return 0;
}

void Application::Impl::drawFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImGuiID dockspace = ImGui::GetID("MainDockSpace");
    if (!layoutChecked || resetLayout) {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace);
        if (resetLayout || node == nullptr || !node->IsSplitNode()) {
            buildDefaultLayout(dockspace); // first start (no saved layout) or user reset
        }
        layoutChecked = true;
        resetLayout = false;
    }
    ImGui::DockSpaceOverViewport(dockspace, ImGui::GetMainViewport());

    drawMainMenu();
    handleShortcuts();
    drawCatalogWindow(state);
    drawParametersWindow(state);
    drawProductWindow(state);
    drawCutListWindow(state);
    drawWarningsWindow(state);
    drawViewportWindow(state, renderer);
    drawAboutWindow();
    if (showImGuiDemo) {
        ImGui::ShowDemoWindow(&showImGuiDemo);
    }
    drawPopups(state);
    updateWindowTitle();

    ImGui::Render();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window.handle, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.10f, 0.10f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // The layout settles over the first frames; then the picture is stable.
    if (config.screenshot && ++frame == 10) {
        writeBackBufferBmp(*config.screenshot, width, height);
        state.quit = true;
    }
    glfwSwapBuffers(window.handle);
}

void Application::Impl::drawMainMenu()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    // No cached reference to the document: "New" / "Open" below may replace it mid-frame.

    if (ImGui::BeginMenu(tr(TextId::MenuFile))) {
        if (ImGui::BeginMenu(tr(TextId::MenuNew))) {
            for (const core::CatalogItem item : core::catalogItems()) {
                if (ImGui::MenuItem(tr(catalogText(item)))) {
                    confirmUnsaved(state, [this, item] { newDocument(state, item); });
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr(TextId::MenuOpen), "Ctrl+O")) {
            confirmUnsaved(state, [this] { openDocument(state); });
        }
        if (ImGui::MenuItem(tr(TextId::MenuSave), "Ctrl+S")) {
            saveDocument(state, false);
        }
        if (ImGui::MenuItem(tr(TextId::MenuSaveAs))) {
            saveDocument(state, true);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu(tr(TextId::MenuProfile))) {
            if (ImGui::MenuItem(tr(TextId::MenuLoadProfile))) {
                loadProfile(state);
            }
            if (ImGui::MenuItem(tr(TextId::MenuSaveProfile))) {
                saveProfile(state);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(TextId::MenuExit), "Alt+F4")) {
            requestQuit();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr(TextId::MenuEdit))) {
        if (ImGui::MenuItem(tr(TextId::MenuUndo), "Ctrl+Z", false, state.document->canUndo())) {
            state.document->undo();
        }
        if (ImGui::MenuItem(tr(TextId::MenuRedo), "Ctrl+Y", false, state.document->canRedo())) {
            state.document->redo();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr(TextId::MenuView))) {
        if (ImGui::MenuItem(tr(TextId::MenuShowFronts), nullptr, &state.sceneOptions.showFronts)
            || ImGui::MenuItem(tr(TextId::MenuShowHoles), nullptr, &state.sceneOptions.showHoles)) {
            state.sceneValid = false;
        }
        if (ImGui::MenuItem(tr(TextId::MenuFitView))) {
            state.fitRequested = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr(TextId::MenuResetLayout))) {
            resetLayout = true;
        }
        ImGui::MenuItem(tr(TextId::MenuShowImGuiDemo), nullptr, &showImGuiDemo);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr(TextId::MenuHelp))) {
        ImGui::MenuItem(tr(TextId::MenuAbout), nullptr, &showAbout);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void Application::Impl::handleShortcuts()
{
    // RouteGlobal: a focused text field keeps its own Ctrl+Z etc.
    constexpr ImGuiInputFlags route = ImGuiInputFlags_RouteGlobal;
    // No cached reference to the document: Ctrl+O may replace it.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, route)) {
        state.document->undo();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, route)
        || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, route)) {
        state.document->redo();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, route)) {
        saveDocument(state, false);
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, route)) {
        confirmUnsaved(state, [this] { openDocument(state); });
    }
    if (ImGui::Shortcut(ImGuiKey_Delete, route)) {
        editor::Document& doc = *state.document;
        if (const core::FurnitureComponent* selected = doc.selected()) {
            const core::ComponentKind kind = selected->kind();
            if (kind == core::ComponentKind::Shelf || kind == core::ComponentKind::Door
                || kind == core::ComponentKind::DrawerStack) {
                const core::ComponentId id = selected->id();
                ui::run(state, [&] { editor::removeComponent(doc, id); });
            }
        }
    }
}

void Application::Impl::drawAboutWindow()
{
    if (!showAbout) {
        return;
    }
    if (ImGui::Begin(tr(TextId::AboutWindow), &showAbout, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(tr(TextId::AboutBody));
        ImGui::Separator();
        ImGui::Text("%s: %s", tr(TextId::CoreVersion), core::versionString().data());
        ImGui::Text("%s: %s", tr(TextId::RendererInfo), glRendererInfo.c_str());
        if (ImGui::Button(tr(TextId::Close))) {
            showAbout = false;
        }
    }
    ImGui::End();
}

void Application::Impl::updateWindowTitle()
{
    const editor::Document& doc = *state.document;
    std::string name;
    if (!doc.path().empty()) {
        name = toUtf8(doc.path().filename());
    } else if (!doc.product().name().empty()) {
        name = doc.product().name();
    } else {
        name = tr(TextId::Untitled);
    }
    const std::string title = name + (doc.isDirty() ? " *" : "") + " — ReadyCAD";
    if (title != windowTitle) {
        windowTitle = title;
        glfwSetWindowTitle(window.handle, windowTitle.c_str());
    }
}

} // namespace rc::ui
