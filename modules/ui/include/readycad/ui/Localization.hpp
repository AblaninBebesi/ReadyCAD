#pragma once

#include <cstddef>
#include <string_view>

namespace rc::ui {

/// Every user-visible string has an id. Code is written in English, the UI speaks Turkish.
///
/// Why an enum instead of string keys ("menu.file"): a typo becomes a compile error, and a test
/// can check that every id has a translation. Adding a language = adding one more table.
enum class TextId : std::size_t {
    AppTitle,
    MenuFile,
    MenuNew,
    MenuOpen,
    MenuSave,
    MenuExit,
    MenuView,
    MenuShowImGuiDemo,
    MenuHelp,
    MenuAbout,
    WelcomeWindow,
    WelcomeHeadline,
    WelcomeBody,
    CoreVersion,
    RendererInfo,
    AboutWindow,
    AboutBody,
    Close,

    Count // keep last
};

enum class Language { Turkish };

void setLanguage(Language language) noexcept;
[[nodiscard]] Language currentLanguage() noexcept;

/// UTF-8 text for the id in the current language. Never returns an empty view for a valid id.
[[nodiscard]] std::string_view text(TextId id) noexcept;

/// Short alias for UI code: ImGui::Button(tr(TextId::Close).data())
[[nodiscard]] inline const char* tr(TextId id) noexcept { return text(id).data(); }

} // namespace rc::ui
