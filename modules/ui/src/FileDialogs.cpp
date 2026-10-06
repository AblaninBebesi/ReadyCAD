#include "FileDialogs.hpp"

#include <nfd.h>

#include <stdexcept>

namespace rc::ui {

namespace {

nfdu8filteritem_t filterFor(FileKind kind)
{
    return kind == FileKind::Project ? nfdu8filteritem_t{"ReadyCAD projesi", "rcproj"}
                                     : nfdu8filteritem_t{"Atölye profili", "rcprofile"};
}

const char* extensionFor(FileKind kind)
{
    return kind == FileKind::Project ? ".rcproj" : ".rcprofile";
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
