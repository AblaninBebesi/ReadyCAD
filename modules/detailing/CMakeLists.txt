# Detailing — implemented in stage 4 (see docs/ARCHITECTURE.md).
# Until then the module is a placeholder that only fixes its place in the dependency graph.
rc_add_module(detailing
    DEPENDS
        readycad::core)
