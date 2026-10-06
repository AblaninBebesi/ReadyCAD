#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace rc::ui {

enum class FileKind {
    Project, // *.rcproj
    Profile, // *.rcprofile — workshop profile
};

/// Native "Open" / "Save as" dialogs (nativefiledialog-extended). nullopt = cancelled.
/// Paths are UTF-8 aware, so Turkish file and folder names work.
[[nodiscard]] std::optional<std::filesystem::path> showOpenDialog(FileKind kind);
[[nodiscard]] std::optional<std::filesystem::path> showSaveDialog(FileKind kind,
                                                                  const std::string& defaultName);

/// UTF-8 std::string for ImGui / GLFW from a path, and back.
[[nodiscard]] std::string toUtf8(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path fromUtf8(const std::string& text);

/// RAII: initialises the dialog library (COM on Windows) for the lifetime of the object.
class FileDialogSession {
public:
    FileDialogSession();
    ~FileDialogSession();
    FileDialogSession(const FileDialogSession&) = delete;
    FileDialogSession& operator=(const FileDialogSession&) = delete;
};

} // namespace rc::ui
