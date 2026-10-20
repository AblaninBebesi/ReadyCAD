# Module `render`

OpenGL 3.3 core renderer: scene built from the model (GL-free, tested), orbit camera, ray picking, RAII wrappers for GPU objects, off-screen MSAA target shown by ImGui.

Kitchen: a panel with cut-outs (the worktop) is drawn as boxes around its openings; own colours for worktop and plinth.

Shapes: shaped panels are prisms (`ScenePrism`) — the face triangulated by ear clipping with hole bridges (`Triangulate`), walls, edges; picking hits triangles.

Decors: `SceneSurface` (picture, grain and width axes, tint); the fragment shader lays the picture in product millimetres along the panel's length; `loadImage` (stb_image), textures loaded once, the decor's average colour when a picture is missing.

Corner kitchens: every part placed by its `placement()` — boxes, prisms, handles, holes and grain axes of a side wall's cabinets turned onto their wall.

**Stages:** 5, kitchen, shapes, decors, corner kitchens
