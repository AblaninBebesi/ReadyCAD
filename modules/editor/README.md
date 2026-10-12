# Module `editor`

GL-free application layer: `Document` (product, selection, undo/redo via JSON snapshots with merging of slider drags, strong exception guarantee, automatic detailing), named UI operations, `PartInfo` for the properties panel. Fully unit-tested.

Stage 9: an edit that would make the product impossible to build (parts in each other's space, a part without size) is refused with `ConstructionError` and undone; `overlaps()` shows the ones a project file brings; `setInCuttingPlan`.

**Stages:** 5, 9
