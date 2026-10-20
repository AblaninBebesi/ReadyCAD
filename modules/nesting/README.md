# Module `nesting`

Cutting sheets into parts (stage 7):

* `Stock` — `StockManager` and `Sheet`: full sheets and remnants per material, what a job may cut from (remnants first, then stock, then sheets to buy), taking used sheets out and remnants in after cutting.
* `Nesting` — parts (`partsFromCutList`: sawn blanks in FlatPart coordinates), settings of the two machines and of the genetic algorithm, `optimize()`, results (sheet layouts, saw cuts, remnants, usage per material).
* Panel saw: guillotine layouts in stages (strips, columns, recuts) with kerf, trims, stacks of identical sheets.
* Nesting centre: true-shape nesting with No-Fit Polygons (Clipper2, integer micrometres), spacing by the cutter diameter, NFP cache with a memory limit.
* Genetic algorithm as in deepnest: order + rotation per part, tournament selection, order crossover, elitism, parallel and reproducible evaluation.
* `NestingRun` — the optimisation on a background thread with progress and cancel (used by the UI).
* `Geometry`, `Nfp` — polygons, quarter-turn transforms, No-Fit Polygon and inner-fit rectangle.

Depends on `core`, `detailing`; Clipper2 is a private dependency. See docs/ARCHITECTURE.md, section "Раскрой и нестинг".

Stage 9: `partsFromCutList` skips the rows left out of the cutting plan (the others keep their row number); `NestingPart` compares with `==`, so a plan can be checked against the current parts.

`NestingSettings::useRemnants` (the "Stoktaki artıkları kullan" checkbox): off, a job takes full sheets only — from stock, then to buy — and leaves the remnants on stock for other jobs.

**Stages:** 7, 9
