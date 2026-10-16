#include "Assets.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <string>
#include <system_error>

namespace rc::ui {

namespace {

std::filesystem::path executableFolder()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2); // a long path: try again with more room
    }
#else
    std::error_code ec;
    const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path{} : self.parent_path();
#endif
}

} // namespace

std::filesystem::path assetsFolder()
{
    std::error_code ec;
    if (const auto next = executableFolder() / "assets"; std::filesystem::is_directory(next, ec)) {
        return next;
    }
    if (const std::filesystem::path source = READYCAD_SOURCE_ASSETS;
        std::filesystem::is_directory(source, ec)) {
        return source;
    }
    return {};
}

} // namespace rc::ui
