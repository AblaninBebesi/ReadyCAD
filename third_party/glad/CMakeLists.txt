# OpenGL 3.3 core loader, generated once with glad2:
#   python -m glad --api gl:core=3.3 --extensions="" --out-path . c
# Committed as generated code so the build does not need Python.
add_library(glad STATIC src/gl.c)
target_include_directories(glad SYSTEM PUBLIC include)
add_library(glad::glad ALIAS glad)
