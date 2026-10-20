# Module `editor`

GL-free application layer: `Document` (product, selection, undo/redo via JSON snapshots with merging of slider drags, strong exception guarantee, automatic detailing), named UI operations, `PartInfo` for the properties panel. Fully unit-tested.

Stage 9: an edit that would make the product impossible to build (parts in each other's space, a part without size) is refused with `ConstructionError` and undone; `overlaps()` shows the ones a project file brings; `setInCuttingPlan`.

Kitchen: carcass operations by carcass id; add, size, move and remove cabinets, kitchen sizes, worktop cut-outs, top style, plinth; an edit that puts a cut-out where it cannot be made is refused like an overlap.

Shapes: `setShape`; a shape that does not fit or takes away material a hole or a groove needs is refused (`shapeConflicts()`).

Corner kitchens: `setSideRun`, `addCabinet` to a wall, `setBlindFiller`; corner cabinets keep their width and place.

**Stages:** 5, 9, kitchen, shapes, corner kitchens
