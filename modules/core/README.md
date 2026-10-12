# Module `core`

Geometric kernel: numeric conventions (mm, double), `Aabb`, materials and `ConstructionRules`, `Panel`, the furniture component tree (Composite) with top-down parametric layout, `WardrobeBuilder` and the catalogue factory.

See docs/ARCHITECTURE.md, section "Ядро".

Stage 9: each structural panel and each drawer box part meets its neighbours with its own thickness (`CarcassFrame`, `DrawerBoxFrame`); panels can be left out of the cutting plan (`PanelComponent::inCuttingPlan`).

**Stages:** 1 (math), 2 (component tree), 3 (parametrics), 9 (per-part thickness, cutting plan flag)
