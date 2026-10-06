# Editor — implemented in stage 5, 9 (see docs/ARCHITECTURE.md).
# Until then the module is a placeholder that only fixes its place in the dependency graph.
rc_add_module(editor
    DEPENDS
        readycad::core
        readycad::detailing
        readycad::nesting
        readycad::export
        readycad::cam)
