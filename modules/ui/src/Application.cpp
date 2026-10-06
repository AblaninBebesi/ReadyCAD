#include "readycad/ui/Application.hpp"

#include "Fonts.hpp"
#include "readycad/core/Version.hpp"
#include "readycad/ui/Localization.hpp"

#include <glad/gl.h>  // must come before GLFW
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <stdexcept>
#include <string>

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
        glfwWindowHint(GLFW_SAMPLES, 4);                       // MSAA: smoother panel edges

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
        float xScale = 1.0f;
        float yScale = 1.0f;
        glfwGetWindowContentScale(window, &xScale, &yScale);
        ImGui::GetStyle().ScaleAllSizes(xScale);
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

} // namespace

struct Application::Impl {
    // Declaration order == construction order; destruction runs in reverse.
    ApplicationConfig config;
    GlfwLibrary glfw;
    GlWindow window;
    ImGuiLayer imgui;
    std::string glRendererInfo;

    bool showWelcome = true;
    bool showAbout = false;
    bool showImGuiDemo = false;

    explicit Impl(const ApplicationConfig& cfg)
        : config(cfg)
        , window(config)
        , imgui(window.handle)
        , glRendererInfo(glString(GL_RENDERER) + " | OpenGL " + glString(GL_VERSION))
    {
    }

    void drawFrame();
    void drawMainMenu();
    void drawWelcomeWindow();
    void drawAboutWindow();
};

Application::Application(ApplicationConfig config) : impl_(std::make_unique<Impl>(config)) {}

Application::~Application() = default;

int Application::run()
{
    GLFWwindow* window = impl_->window.handle;
    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();
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

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    drawMainMenu();
    drawWelcomeWindow();
    drawAboutWindow();
    if (showImGuiDemo) {
        ImGui::ShowDemoWindow(&showImGuiDemo);
    }

    ImGui::Render();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window.handle, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.16f, 0.17f, 0.19f, 1.0f); // 3D viewport background (stage 5: scene goes here)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window.handle);
}

void Application::Impl::drawMainMenu()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu(tr(TextId::MenuFile))) {
        ImGui::MenuItem(tr(TextId::MenuNew), "Ctrl+N", false, false);  // stage 5
        ImGui::MenuItem(tr(TextId::MenuOpen), "Ctrl+O", false, false); // stage 5
        ImGui::MenuItem(tr(TextId::MenuSave), "Ctrl+S", false, false); // stage 5
        ImGui::Separator();
        if (ImGui::MenuItem(tr(TextId::MenuExit), "Alt+F4")) {
            glfwSetWindowShouldClose(window.handle, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr(TextId::MenuView))) {
        ImGui::MenuItem(tr(TextId::WelcomeWindow), nullptr, &showWelcome);
        ImGui::MenuItem(tr(TextId::MenuShowImGuiDemo), nullptr, &showImGuiDemo);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr(TextId::MenuHelp))) {
        ImGui::MenuItem(tr(TextId::MenuAbout), nullptr, &showAbout);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void Application::Impl::drawWelcomeWindow()
{
    if (!showWelcome) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(520, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(tr(TextId::WelcomeWindow), &showWelcome)) {
        ImGui::TextUnformatted(tr(TextId::WelcomeHeadline));
        ImGui::Separator();
        ImGui::TextWrapped("%s", tr(TextId::WelcomeBody));
        ImGui::Spacing();
        ImGui::Text("%s: %s", tr(TextId::CoreVersion), core::versionString().data());
        ImGui::Text("%s: %s", tr(TextId::RendererInfo), glRendererInfo.c_str());
    }
    ImGui::End();
}

void Application::Impl::drawAboutWindow()
{
    if (!showAbout) {
        return;
    }
    if (ImGui::Begin(tr(TextId::AboutWindow), &showAbout, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(tr(TextId::AboutBody));
        if (ImGui::Button(tr(TextId::Close))) {
            showAbout = false;
        }
    }
    ImGui::End();
}

} // namespace rc::ui
