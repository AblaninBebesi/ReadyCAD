# Module `storage`

JSON files (nlohmann/json):

* projects (`*.rcproj`) and workshop profiles (`*.rcprofile`): the design, not derived geometry or drilling; missing fields fall back to defaults (stage 5);
* the stock of sheets and remnants (`stock.rcstock`) and the settings of the machines (`settings.json`: cutting, stage 7; the CNC nesting centre with its tools and G-code dialect, stage 8) in the user folder, `userDataFolder()`.

Every file is written to `<name>.tmp` and then renamed over the old one, so a crash never leaves a half-written file.

Stage 9: `WorkshopFiles` (stock and settings of a workshop in one folder, missing files give defaults, broken ones an error); parts left out of the cutting plan; the choices of drawer box parts are saved too.

Kitchen: project format 2 — `"kitchen"` with sizes, worktop and the cabinets of both rows; carcasses with `topStyle`, `plinth`, `width`, `cutout`. Version 1 files still open.

Shapes: project format 3 — `"shape"` of a part. Decors: project format 4, profile format 2 — the decor catalogue and `decor` of materials.

Corner kitchens: project format 5 — side runs under `"left"` / `"right"` with their corners, `blindFiller` of a carcass.

`worktopJoint` of a carcass, `worktopPieces` (choices of the pieces after the first).

L corners: `"type": "L"` of a corner, legs, shelves, part choices and the folding door's handle of an L cabinet.

**Stages:** 5, 7, 8, 9, kitchen, shapes, corner kitchens
