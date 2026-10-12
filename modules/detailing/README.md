# Module `detailing`

Turns the geometric model into manufacturable parts: cut sizes (finish - edge + pre-milling), cut list and edge banding usage, joints found from geometry, System 32 drilling (confirmat / eccentric / dowel connectors, shelf supports, hinge cups and plates), back panel grooves. Stage 6: `FlatPart` (a panel unfolded as seen from one face, never mirrored; base of DXF, PDF drawings, nesting and CAM) and the hardware list.

See docs/ARCHITECTURE.md, section "Деталировка и присадка".

Stage 9: a part left out of the cutting plan gets a cut list row of its own (`CutListRow::inCuttingPlan`).

**Stages:** 4, 6, 9
