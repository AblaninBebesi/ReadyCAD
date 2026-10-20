# Module `detailing`

Turns the geometric model into manufacturable parts: cut sizes (finish - edge + pre-milling), cut list and edge banding usage, joints found from geometry, System 32 drilling (confirmat / eccentric / dowel connectors, shelf supports, hinge cups and plates), back panel grooves. Stage 6: `FlatPart` (a panel unfolded as seen from one face, never mirrored; base of DXF, PDF drawings, nesting and CAM) and the hardware list.

See docs/ARCHITECTURE.md, section "Деталировка и присадка".

Stage 9: a part left out of the cutting plan gets a cut list row of its own (`CutListRow::inCuttingPlan`).

Kitchen: detailing carcass by carcass (no joint to the neighbouring cabinet), rails take connectors, back grooves only in parts that hold the back; legs, plinth clips and hangers; `FlatPart::cutouts` and `cutoutOutline()`. Cut list rows group parts with the same machining (holes, grooves, cut-outs as the drawing shows them) and the same hand of mirror parts.

Shapes: `FlatPart::contour` and `blank` of shaped parts; `findShapeConflicts` — holes and grooves must stay in material.

L corner cabinets: joints, shelf supports in both end walls, the first door on its wall, folding hinge cups in the second; folding hinges, legs and hangers in the hardware list (and the hangers of a blind wall corner cabinet).

**Stages:** 4, 6, 9, kitchen, shapes, corner kitchens
