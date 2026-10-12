#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace rc::ui {

enum class FileKind {
    Project, // *.rcproj
    Profile, // *.rcprofile — workshop profile
    Pdf,     // documentation
    Dxf,     // drawings for CAD/CAM
    Csv,     // cut list for the saw's optimiser or a spreadsheet
};

/// Native "Open" / "Save as" dialogs (nativefiledialog-extended). nullopt = cancelled.
/// Paths are UTF-8 aware, so Turkish file and folder names work.
[[nodiscard]] std::optional<std::filesystem::path> showOpenDialog(FileKind kind);
[[nodiscard]] std::optional<std::filesystem::path> showSaveDialog(FileKind kind,
                                                                  const std::string& defaultName);

/// Native "Choose folder" dialog. nullopt = cancelled.
[[nodiscard]] std::optional<std::filesystem::path> showFolderDialog();

/// Opens a file or folder with the program the system uses for it (PDF viewer, Explorer).
/// Returns false if that is not possible.
bool openWithSystem(const std::filesystem::path& path);

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
