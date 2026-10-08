# Module `i18n`

User-visible texts: the `TextId` catalogue with its Turkish table (`Localization`) and names of
model things — component kinds, catalogue items, hole types, numbered part names, lengths in mm
(`Labels`), UTF-8 helpers and the Turkish-to-ASCII fold for files read by machines (`Utf8`).
Shared by the UI, exported documents (PDF, DXF) and CNC programs, so it sits right above `core`.

**Stages:** 1 (in `ui`), 6 (moved into its own module for the PDF export), 7 (cutting texts), 8 (CNC texts, `Utf8` moved here from `export`)
