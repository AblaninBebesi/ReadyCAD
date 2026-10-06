# Render — implemented in stage 5 (see docs/ARCHITECTURE.md).
# Until then the module is a placeholder that only fixes its place in the dependency graph.
rc_add_module(render
    DEPENDS
        readycad::core
        glad::glad
        OpenGL::GL)
