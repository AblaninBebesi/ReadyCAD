#pragma once

#include "readycad/render/Camera.hpp"
#include "readycad/render/GlObjects.hpp"
#include "readycad/render/Scene.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rc::render {

/// Draws a Scene into an off-screen texture (shown by the UI in its 3D view window).
///
/// Panels are drawn as shaded boxes with dark outlines (the usual furniture-CAD look), holes as
/// dark discs, plus a floor grid. Boxes are one unit cube scaled per item, so a change of the
/// model needs no GPU buffer updates for them; only the few shaped panels (notches, openings)
/// are uploaded as their own meshes, once per scene. Requires a current OpenGL 3.3 context.
class Renderer {
public:
    Renderer();

    /// Where the decor pictures are (assets/textures). Pictures are loaded when first drawn.
    void setTextureFolder(std::filesystem::path folder);

    /// Renders into a target of the given size and returns the colour texture.
    GLuint render(const Scene& scene, const OrbitCamera& camera, int width, int height);

private:
    void drawGrid(const Scene& scene, const glm::mat4& viewProjection);
    /// Uploads the shaped panels of a scene it has not drawn yet.
    void uploadPrisms(const Scene& scene);
    /// The GPU texture of a decor picture, loaded once; 0 if it cannot be read.
    GLuint texture(const std::string& file);
    /// Sets the shader up for the surface of the next item: its picture, or its plain colour.
    void useSurface(const SceneSurface& surface);

    ShaderProgram shader_;
    GLint uModel_;
    GLint uViewProj_;
    GLint uNormalMatrix_;
    GLint uColor_;
    GLint uLit_;
    GLint uLight1_;
    GLint uLight2_;
    GLint uTextured_;
    GLint uTexture_;
    GLint uTint_;
    GLint uGrain_;
    GLint uAcross_;
    GLint uTile_;

    Mesh cube_;
    Mesh cubeEdges_;
    Mesh disc_;
    std::unique_ptr<Mesh> grid_;
    std::vector<std::unique_ptr<Mesh>> prismFaces_;
    std::vector<std::unique_ptr<Mesh>> prismEdges_;
    std::uint64_t prismSerial_ = 0;
    std::filesystem::path textureFolder_;
    std::map<std::string, GlTexture> textures_;
    float gridExtent_ = 0.0f;
    OffscreenTarget target_;
};

} // namespace rc::render
