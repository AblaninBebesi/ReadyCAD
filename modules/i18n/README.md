# Module `i18n`

User-visible texts: the `TextId` catalogue with its Turkish, Russian and English tables, the
language of the program and of the documents (`LanguageScope`), templates (`format`) and the
forms of counted words (`countText`) (`Localization`); names of model things — component kinds,
catalogue items, hole types, numbered part names, lengths in mm, percentages (`Labels`); UTF-8
helpers and the Turkish and Cyrillic fold to ASCII for files read by machines (`Utf8`).
Shared by the UI, exported documents (PDF, DXF) and CNC programs, so it sits right above `core`.

**Stages:** 1 (in `ui`), 6 (moved into its own module for the PDF export), 7 (cutting texts), 8 (CNC texts, `Utf8` moved here from `export`), 9 (production package and summary texts, `safeFileName`), kitchen (kitchen texts, tree names of rows, cabinets and rails, `partName` with the hand of mirror parts), shapes (`featureText`, `cornerText`), languages (Russian and English tables, `format`, `countText`, `mmUnit`, `percent`, Cyrillic in `asciiFold`)
