# Module `render`

OpenGL 3.3 core renderer: scene built from the model (GL-free, tested), orbit camera, ray picking, RAII wrappers for GPU objects, off-screen MSAA target shown by ImGui.

Kitchen: a panel with cut-outs (the worktop) is drawn as boxes around its openings; own colours for worktop and plinth.

Shapes: shaped panels are prisms (`ScenePrism`) — the face triangulated by ear clipping with hole bridges (`Triangulate`), walls, edges; picking hits triangles.

**Stages:** 5, kitchen, shapes
