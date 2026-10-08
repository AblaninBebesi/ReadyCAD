# Module `cam`

CNC programs (stage 8), in three layers:

* `Operations` — what to machine, independent of any machine: holes in any of the six faces, grooves, contours of a part (`partMachining`), moved onto a sheet with its nesting transform (`sheetJob`).
* `NestingCenter` — how a CNC nesting centre makes them (`planSheet`): drilling head first (drill by drill, short routes), holes without a drill and grooves with routers, contours last (small parts first, climb milling, passes, tool radius offset); what it cannot do from above is reported (`Leftover`). Machine profile: tool table, heights, controller dialect.
* `Program` + `Postprocessor` — toolpaths independent of the controller, written by `GCodePostprocessor` in a configurable ISO G-code dialect (presets "ISO G-code" and "KDT (NcStudio)").
* `Route` — nearest neighbour + 2-opt for short travel between holes.

New controllers are new dialects (data) or new postprocessors; new machine types (horizontal drilling, panel saw, edgebander) are new planners over the same operations. See docs/ARCHITECTURE.md, section "Программы для ЧПУ".

**Stage:** 8
