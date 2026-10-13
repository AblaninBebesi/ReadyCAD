# Module `core`

Geometric kernel: numeric conventions (mm, double), `Aabb`, materials and `ConstructionRules`, `Panel`, the furniture component tree (Composite) with top-down parametric layout, `WardrobeBuilder` and the catalogue factory.

See docs/ARCHITECTURE.md, section "Ядро".

Stage 9: each structural panel and each drawer box part meets its neighbours with its own thickness (`CarcassFrame`, `DrawerBoxFrame`); panels can be left out of the cutting plan (`PanelComponent::inCuttingPlan`).

Kitchen: a product holds one carcass or one `Kitchen` — `CabinetRow`s of cabinets (fixed or automatic widths, `distributeSizes()` shared with `Section`), `Worktop` with cut-outs, cabinets with `Rail`s (flat or upright) instead of a top panel and a `Plinth`; `KitchenBuilder` with six modules; `Kitchen::misplacedCutouts()`; `Panel::cutouts()`.

**Stages:** 1 (math), 2 (component tree), 3 (parametrics), 9 (per-part thickness, cutting plan flag), kitchen
