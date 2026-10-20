# Module `ui`

Dear ImGui windows on top of `editor` + `render`; main window and frame loop (`Application`). Texts come from the `i18n` module.

**Stages:** 1 (window), 5 (catalogue, parameters, component tree, properties), 6 (export menu, PDF settings, file and folder dialogs), 7 (cutting plan with live progress, stock of sheets, cutting settings; `NestingPanels.cpp`), 8 (CNC settings, program export, toolpath preview; `CncPanels.cpp`), 9 (every export goes through `production`; production package with a cutting plan computed first if needed; "Kesim" column and checkbox; overlaps in the warnings window), kitchen (kitchen sizes, add a cabinet, cabinet properties, tree names), shapes ("Kesimler": part sketch, features, add and remove), decors ("Desenler", part decor, `assetsFolder()`), corner kitchens ("Sol duvar" / "Sağ duvar", the wall of a new cabinet, corner widths and fillers)
