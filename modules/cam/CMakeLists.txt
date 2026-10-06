# CAM — implemented in stage 8 (see docs/ARCHITECTURE.md).
# Until then the module is a placeholder that only fixes its place in the dependency graph.
rc_add_module(cam
    DEPENDS
        readycad::core
        readycad::detailing
        readycad::nesting)
