#include "FileDialogs.hpp"

#include <nfd.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <stdexcept>

namespace rc::ui {

namespace {

nfdu8filteritem_t filterFor(FileKind kind)
{
    switch (kind) {
    case FileKind::Project: return {"ReadyCAD projesi", "rcproj"};
    case FileKind::Profile: return {"Atölye profili", "rcprofile"};
    case FileKind::Pdf:     return {"PDF belgesi", "pdf"};
    case FileKind::Dxf:     return {"DXF çizimi", "dxf"};
    }
    return {"ReadyCAD projesi", "rcproj"};
}

const char* extensionFor(FileKind kind)
{
    switch (kind) {
    case FileKind::Project: return ".rcproj";
    case FileKind::Profile: return ".rcprofile";
    case FileKind::Pdf:     return ".pdf";
    case FileKind::Dxf:     return ".dxf";
    }
    return ".rcproj";
}

} // namespace

std::string toUtf8(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::filesystem::path fromUtf8(const std::string& text)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),
                                               text.size()));
}

std::optional<std::filesystem::path> showOpenDialog(FileKind kind)
{
    const nfdu8filteritem_t filter = filterFor(kind);
    nfdu8char_t* out = nullptr;
    if (NFD_OpenDialogU8(&out, &filter, 1, nullptr) != NFD_OKAY) {
        return std::nullopt;
    }
    std::filesystem::path path = fromUtf8(out);
    NFD_FreePathU8(out);
    return path;
}

std::optional<std::filesystem::path> showSaveDialog(FileKind kind, const std::string& defaultName)
{
    const nfdu8filteritem_t filter = filterFor(kind);
    nfdu8char_t* out = nullptr;
    if (NFD_SaveDialogU8(&out, &filter, 1, nullptr, defaultName.c_str()) != NFD_OKAY) {
        return std::nullopt;
    }
    std::filesystem::path path = fromUtf8(out);
    NFD_FreePathU8(out);
    if (path.extension() != extensionFor(kind)) {
        path += extensionFor(kind);
    }
    return path;
}

std::optional<std::filesystem::path> showFolderDialog()
{
    nfdu8char_t* out = nullptr;
    if (NFD_PickFolderU8(&out, nullptr) != NFD_OKAY) {
        return std::nullopt;
    }
    std::filesystem::path path = fromUtf8(out);
    NFD_FreePathU8(out);
    return path;
}

bool openWithSystem(const std::filesystem::path& path)
{
#ifdef _WIN32
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32; // ShellExecute's way of saying "success"
#else
    (void)path; // TODO(stage 10): xdg-open / open on Linux and macOS
    return false;
#endif
}

FileDialogSession::FileDialogSession()
{
    if (NFD_Init() != NFD_OKAY) {
        throw std::runtime_error("Could not initialise the file dialogs");
    }
}

FileDialogSession::~FileDialogSession()
{
    NFD_Quit();
}

} // namespace rc::ui
