# Module `production`

The production pipeline (stage 9): one path from a designed product to the files of the workshop, used by the menus, the command line and the tests alike.

* `Pipeline` — an `Order` (document × quantity) → `cuttingParts` (without the parts left out of the cutting plan) → `planCutting` → single outputs (PDF, part and sheet DXF, CNC programs, CSV cut list) → `writePackage`: all of it in one new folder with `OZET.txt`, all or nothing. `planMatches` refuses files from a stale cutting plan.
* `Summary` — texts for people: the package summary (columns aligned by letters, in the language of the moment), machining times, work left for other machines, the message after a package.
* `Batch` — the command line without a window (`--package`): opens the project, plans with the workshop's stock and settings, writes the package, prints the summary; the stock is only read.

Languages: the package and its file names (`URETIM` / `PROIZVODSTVO` / `PRODUCTION`, `OZET` / `SVODKA` / `SUMMARY`, ...) are in the language of the moment; the CSV uses `;` and a decimal comma for Turkish and Russian spreadsheets, `,` and a point for English ones; `BatchOptions::language` (`--lang`) or the documents' language of the settings file.

See docs/ARCHITECTURE.md, section "Сквозной конвейер".

**Stages:** 9, languages
