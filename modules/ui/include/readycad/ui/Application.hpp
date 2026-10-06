#pragma once

#include <memory>

namespace rc::ui {

struct ApplicationConfig {
    int windowWidth = 1600;
    int windowHeight = 900;
    bool vsync = true;
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
