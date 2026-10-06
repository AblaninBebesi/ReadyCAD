#include "readycad/ui/Localization.hpp"

#include <array>

namespace rc::ui {

namespace {

constexpr std::size_t kTextCount = static_cast<std::size_t>(TextId::Count);
using Table = std::array<std::string_view, kTextCount>;

// Turkish (tr-TR). Strings are UTF-8; keep the order identical to the TextId enum.
// "###id" suffixes keep ImGui window ids stable if the visible title ever changes.
constexpr Table kTurkish = {
    "ReadyCAD — Parametrik Mobilya Tasarımı",          // AppTitle
    "Dosya",                                           // MenuFile
    "Yeni",                                            // MenuNew
    "Aç...",                                           // MenuOpen
    "Kaydet",                                          // MenuSave
    "Çıkış",                                           // MenuExit
    "Görünüm",                                         // MenuView
    "ImGui demo penceresi",                            // MenuShowImGuiDemo
    "Yardım",                                          // MenuHelp
    "Hakkında",                                        // MenuAbout
    "Hoş geldiniz###welcome",                          // WelcomeWindow
    "ReadyCAD'e hoş geldiniz!",                        // WelcomeHeadline
    "Gövde mobilyası için parametrik tasarım, detaylandırma, "
    "kesim optimizasyonu ve CNC programı üretimi.",    // WelcomeBody
    "Çekirdek sürümü",                                 // CoreVersion
    "Grafik sürücüsü",                                 // RendererInfo
    "Hakkında###about",                                // AboutWindow
    "ReadyCAD — diploma projesi.\nC++20, OpenGL 3.3, Dear ImGui.", // AboutBody
    "Kapat",                                           // Close
};

Language gLanguage = Language::Turkish;

const Table& tableFor(Language language) noexcept
{
    switch (language) {
    case Language::Turkish:
        return kTurkish;
    }
    return kTurkish;
}

} // namespace

void setLanguage(Language language) noexcept
{
    gLanguage = language;
}

Language currentLanguage() noexcept
{
    return gLanguage;
}

std::string_view text(TextId id) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    if (index >= kTextCount) {
        return "???";
    }
    return tableFor(gLanguage)[index];
}

} // namespace rc::ui
