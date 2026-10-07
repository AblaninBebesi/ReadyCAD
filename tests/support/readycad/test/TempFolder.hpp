#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace rc::test {

/// A folder in the system temp directory, empty at the start, removed at the end of the test.
class TempFolder {
public:
    explicit TempFolder(const std::string& name)
        : path_(std::filesystem::temp_directory_path() / ("readycad_test_" + name))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~TempFolder()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

/// The whole file as bytes.
inline std::string readFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

} // namespace rc::test
