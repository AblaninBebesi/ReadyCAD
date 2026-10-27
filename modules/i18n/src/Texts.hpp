#pragma once

// The tables of the languages other than Turkish (Localization.cpp has the Turkish one), each in
// a file of its own: a translator works on one file.

#include "readycad/i18n/Localization.hpp"

#include <span>
#include <string_view>
#include <utility>

namespace rc::i18n::detail {

using Entry = std::pair<TextId, std::string_view>;

[[nodiscard]] std::span<const Entry> russianTexts() noexcept;
[[nodiscard]] std::span<const Entry> englishTexts() noexcept;

} // namespace rc::i18n::detail
