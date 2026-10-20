# Module `core`

Geometric kernel: numeric conventions (mm, double), `Aabb`, materials and `ConstructionRules`, `Panel`, the furniture component tree (Composite) with top-down parametric layout, `WardrobeBuilder` and the catalogue factory.

See docs/ARCHITECTURE.md, section "Ядро".

Stage 9: each structural panel and each drawer box part meets its neighbours with its own thickness (`CarcassFrame`, `DrawerBoxFrame`); panels can be left out of the cutting plan (`PanelComponent::inCuttingPlan`).

Kitchen: a product holds one carcass or one `Kitchen` — `CabinetRow`s of cabinets (fixed or automatic widths, `distributeSizes()` shared with `Section`), `Worktop` with cut-outs, cabinets with `Rail`s (flat or upright) instead of a top panel and a `Plinth`; `KitchenBuilder` with six modules; `Kitchen::misplacedCutouts()`; `Panel::cutouts()`.

Shapes: `PanelShape` — notches, shaped corners and openings bound to the edges of a panel (`ShapeFeature`), the outline built from them (`panelOutline`, also grown for the blank), `shapeFits`; `Polygon` helpers.

Decors: `Decor`, `defaultDecorCatalog()`, `withDecor()` (the decor's name in the material's name, the grain from the decor); `Material::decor`, `ConstructionRules::decors`.

Corner kitchens: `Placement` (quarter turns about Y); `FurnitureComponent::placement()` / `productBox()`; `KitchenRun` (a wall's rows and worktop in its own coordinates), `KitchenCorner`, blind corner cabinets (`Carcass::setBlindCorner`, `BlindPanel`), `makeCorner()`, `KitchenBuilder::sideWall()`; catalogue "Mutfak (L)", "Mutfak (U)"; collisions in product coordinates.

**Stages:** 1 (math), 2 (component tree), 3 (parametrics), 9 (per-part thickness, cutting plan flag), kitchen, shapes, decors, corner kitchens
