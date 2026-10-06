# Nesting — implemented in stage 7 (see docs/ARCHITECTURE.md).
# Until then the module is a placeholder that only fixes its place in the dependency graph.
rc_add_module(nesting
    DEPENDS
        readycad::core
        readycad::detailing)
