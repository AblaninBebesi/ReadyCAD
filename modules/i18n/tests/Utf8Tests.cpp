// UTF-8 helpers: decoding, Turkish letters to ASCII for files read by machines.

#include "readycad/i18n/Utf8.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rc::i18n;

TEST_CASE("UTF-8 is decoded into code points", "[i18n][utf8]")
{
    CHECK(decodeUtf8("Raf") == U"Raf");
    CHECK(decodeUtf8("Çekmece yanı") == U"Çekmece yanı");
    CHECK(decodeUtf8("⌀5") == U"⌀5");
    CHECK(decodeUtf8("\xC3") == U"�");     // cut off in the middle of a letter
    CHECK(decodeUtf8("a\xFFz") == U"a�z"); // not UTF-8 at all
}

TEST_CASE("Turkish letters fold to ASCII, other symbols to underscores", "[i18n][utf8]")
{
    CHECK(asciiFold("Çekmece yanı, Şifonyer, İğne, Ölçü, Üst") ==
          "Cekmece yani, Sifonyer, Igne, Olcu, Ust");
    CHECK(asciiFold("Raf ⌀5") == "Raf _5");
    CHECK(asciiFold("") == "");
}

TEST_CASE("Names become file names that work everywhere", "[i18n][utf8]")
{
    CHECK(safeFileName("Gardırop 1/2") == "Gardirop_1_2");
    CHECK(safeFileName("Mutfak: köşe*") == "Mutfak__kose_");
    CHECK(safeFileName("A-12") == "A-12");
}

TEST_CASE("Paths come out as UTF-8", "[i18n][utf8]")
{
    const std::filesystem::path path(u8"Şifonyer.nc");
    CHECK(pathToUtf8(path) == "Şifonyer.nc");
}
