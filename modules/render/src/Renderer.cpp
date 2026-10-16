#include "readycad/render/Renderer.hpp"

#include "readycad/render/Image.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace rc::render {

namespace {

constexpr const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMatrix;
out vec3 vNormal;
out vec3 vWorld;
void main() {
    vNormal = uNormalMatrix * aNormal;
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorld = world.xyz;
    gl_Position = uViewProj * world;
}
)";

// A decor picture is laid in millimetres of the product: left to right along the panel's
// length (the grain), on the faces up along its width, on the edges along its thickness.
constexpr const char* kFragmentShader = R"(#version 330 core
in vec3 vNormal;
in vec3 vWorld;
uniform vec4 uColor;
uniform int uLit;
uniform vec3 uLight1;
uniform vec3 uLight2;
uniform int uTextured;
uniform sampler2D uTexture;
uniform vec4 uTint;
uniform vec3 uGrain;
uniform vec3 uAcross;
uniform float uTile;
out vec4 fragColor;
void main() {
    if (uLit == 0) { fragColor = uColor; return; }
    vec3 n = normalize(vNormal);
    vec3 base = uColor.rgb;
    if (uTextured == 1) {
        vec3 thick = cross(uGrain, uAcross);
        vec3 u = uGrain;
        vec3 v = uAcross;
        if (abs(dot(n, uGrain)) > 0.5) { u = uAcross; v = thick; }  // an end of the panel
        else if (abs(dot(n, uAcross)) > 0.5) { v = thick; }        // a long edge
        vec2 uv = vec2(dot(vWorld, u), dot(vWorld, v)) / uTile;
        base = texture(uTexture, uv).rgb * uTint.rgb;
    }
    float light = 0.38 + 0.50 * max(dot(n, uLight1), 0.0) + 0.22 * max(dot(n, uLight2), 0.0);
    fragColor = vec4(base * light, uColor.a);
}
)";

/// How large one picture is on the furniture, mm.
constexpr float kTextureTile = 800.0f;

/// Unit cube [0,1]^3 as 12 triangles with face normals.
std::vector<float> cubeVertices()
{
    struct Face {
        glm::vec3 normal, origin, u, v;
    };
    const Face faces[] = {
        {{1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}},  {{-1, 0, 0}, {0, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}},  {{0, -1, 0}, {0, 0, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {0, 0, 0}, {0, 1, 0}, {1, 0, 0}},
    };
    std::vector<float> data;
    for (const Face& f : faces) {
        const glm::vec3 p[4] = {f.origin, f.origin + f.u, f.origin + f.u + f.v, f.origin + f.v};
        for (const int i : {0, 1, 2, 0, 2, 3}) {
            data.insert(data.end(), {p[i].x, p[i].y, p[i].z, f.normal.x, f.normal.y, f.normal.z});
        }
    }
    return data;
}

/// The 12 edges of the unit cube as line segments.
std::vector<float> cubeEdgeVertices()
{
    std::vector<float> data;
    for (int axis = 0; axis < 3; ++axis) {
        for (int a = 0; a < 2; ++a) {
            for (int b = 0; b < 2; ++b) {
                glm::vec3 from(0.0f);
                from[(axis + 1) % 3] = static_cast<float>(a);
                from[(axis + 2) % 3] = static_cast<float>(b);
                glm::vec3 to = from;
                to[axis] = 1.0f;
                data.insert(data.end(), {from.x, from.y, from.z, 0, 0, 0, to.x, to.y, to.z, 0, 0, 0});
            }
        }
    }
    return data;
}

/// Unit disc in the XY plane facing +Z.
std::vector<float> discVertices()
{
    constexpr int kSegments = 20;
    std::vector<float> data;
    for (int i = 0; i < kSegments; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / kSegments;
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / kSegments;
        data.insert(data.end(), {0, 0, 0, 0, 0, 1});
        data.insert(data.end(), {std::cos(a0), std::sin(a0), 0, 0, 0, 1});
        data.insert(data.end(), {std::cos(a1), std::sin(a1), 0, 0, 0, 1});
    }
    return data;
}

Mesh makeMesh(const std::vector<float>& vertices, GLenum primitive)
{
    return Mesh(vertices.data(), static_cast<GLsizei>(vertices.size() / 6), primitive);
}

glm::mat4 boxMatrix(const glm::vec3& min, const glm::vec3& max)
{
    const glm::vec3 size = glm::max(max - min, glm::vec3(0.01f));
    return glm::scale(glm::translate(glm::mat4(1.0f), min), size);
}

glm::mat4 discMatrix(const SceneHole& hole)
{
    const glm::vec3 z = glm::normalize(hole.normal);
    const glm::vec3 helper = std::abs(z.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    const glm::vec3 x = glm::normalize(glm::cross(helper, z));
    const glm::vec3 y = glm::cross(z, x);
    const glm::mat4 basis(glm::vec4(x, 0), glm::vec4(y, 0), glm::vec4(z, 0),
                          glm::vec4(hole.center, 1));
    return glm::scale(basis, glm::vec3(hole.radius));
}

glm::mat3 normalMatrix(const glm::mat4& model)
{
    return glm::transpose(glm::inverse(glm::mat3(model)));
}

constexpr glm::vec4 kBackground{0.19f, 0.20f, 0.22f, 1.0f};
constexpr glm::vec4 kOutline{0.12f, 0.11f, 0.10f, 1.0f};
constexpr glm::vec4 kSelectedOutline{1.0f, 0.55f, 0.10f, 1.0f};
constexpr glm::vec4 kHole{0.10f, 0.08f, 0.07f, 1.0f};
constexpr glm::vec4 kGrid{0.30f, 0.31f, 0.34f, 1.0f};

} // namespace

Renderer::Renderer()
    : shader_(kVertexShader, kFragmentShader)
    , uModel_(shader_.location("uModel"))
    , uViewProj_(shader_.location("uViewProj"))
    , uNormalMatrix_(shader_.location("uNormalMatrix"))
    , uColor_(shader_.location("uColor"))
    , uLit_(shader_.location("uLit"))
    , uLight1_(shader_.location("uLight1"))
    , uLight2_(shader_.location("uLight2"))
    , uTextured_(shader_.location("uTextured"))
    , uTexture_(shader_.location("uTexture"))
    , uTint_(shader_.location("uTint"))
    , uGrain_(shader_.location("uGrain"))
    , uAcross_(shader_.location("uAcross"))
    , uTile_(shader_.location("uTile"))
    , cube_(makeMesh(cubeVertices(), GL_TRIANGLES))
    , cubeEdges_(makeMesh(cubeEdgeVertices(), GL_LINES))
    , disc_(makeMesh(discVertices(), GL_TRIANGLES))
{
}

void Renderer::setTextureFolder(std::filesystem::path folder)
{
    textureFolder_ = std::move(folder);
    textures_.clear();
}

GLuint Renderer::texture(const std::string& file)
{
    auto it = textures_.find(file);
    if (it == textures_.end()) {
        // Loaded once; a picture that cannot be read is remembered as missing (plain colour).
        GlTexture loaded;
        if (const auto image = loadImage(textureFolder_ / file)) {
            GLuint name = 0;
            glGenTextures(1, &name);
            loaded = GlTexture(name);
            glBindTexture(GL_TEXTURE_2D, name);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, image->width, image->height, 0, GL_RGB,
                         GL_UNSIGNED_BYTE, image->rgb.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        it = textures_.emplace(file, std::move(loaded)).first;
    }
    return it->second.get();
}

void Renderer::useSurface(const SceneSurface& surface)
{
    const GLuint name = surface.texture.empty() ? 0 : texture(surface.texture);
    shader_.set(uTextured_, name != 0 ? 1 : 0);
    if (name != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, name);
        shader_.set(uTexture_, 0);
        shader_.set(uTint_, surface.tint);
        shader_.set(uGrain_, surface.grain);
        shader_.set(uAcross_, surface.across);
    }
}

void Renderer::uploadPrisms(const Scene& scene)
{
    if (scene.serial == prismSerial_ && prismFaces_.size() == scene.prisms.size()) {
        return; // the same scene: its prisms are on the GPU already
    }
    prismFaces_.clear();
    prismEdges_.clear();
    for (const ScenePrism& prism : scene.prisms) {
        std::vector<float> faces;
        for (std::size_t i = 0; i < prism.triangles.size(); ++i) {
            const glm::vec3& p = prism.triangles[i];
            const glm::vec3& n = prism.normals[i / 3];
            faces.insert(faces.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
        }
        std::vector<float> edges;
        for (const glm::vec3& p : prism.lines) {
            edges.insert(edges.end(), {p.x, p.y, p.z, 0.0f, 0.0f, 0.0f});
        }
        prismFaces_.push_back(std::make_unique<Mesh>(makeMesh(faces, GL_TRIANGLES)));
        prismEdges_.push_back(std::make_unique<Mesh>(makeMesh(edges, GL_LINES)));
    }
    prismSerial_ = scene.serial;
}

void Renderer::drawGrid(const Scene& scene, const glm::mat4& viewProjection)
{
    // A 100 mm floor grid a bit larger than the product; rebuilt only when it must grow.
    const glm::vec3 size(static_cast<float>(scene.bounds.size().x),
                         0.0f, static_cast<float>(scene.bounds.size().z));
    const float extent = std::ceil((std::max(size.x, size.z) + 2000.0f) / 1000.0f) * 1000.0f;
    if (!grid_ || extent != gridExtent_) {
        std::vector<float> lines;
        for (float v = -extent; v <= extent + 0.5f; v += 100.0f) {
            lines.insert(lines.end(), {v, 0, -extent, 0, 0, 0, v, 0, extent, 0, 0, 0});
            lines.insert(lines.end(), {-extent, 0, v, 0, 0, 0, extent, 0, v, 0, 0, 0});
        }
        grid_ = std::make_unique<Mesh>(makeMesh(lines, GL_LINES));
        gridExtent_ = extent;
    }
    const glm::vec3 center(static_cast<float>(scene.bounds.center().x), 0.0f,
                           static_cast<float>(scene.bounds.center().z));
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::round(center / 100.0f) * 100.0f);
    shader_.set(uViewProj_, viewProjection);
    shader_.set(uModel_, model);
    shader_.set(uColor_, kGrid);
    shader_.set(uLit_, 0);
    grid_->draw();
}

GLuint Renderer::render(const Scene& scene, const OrbitCamera& camera, int width, int height)
{
    target_.resize(width, height);
    target_.bind();
    glClearColor(kBackground.r, kBackground.g, kBackground.b, kBackground.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    const float aspect = static_cast<float>(target_.width()) / static_cast<float>(target_.height());
    const glm::mat4 viewProjection = camera.projection(aspect) * camera.view();

    shader_.use();
    shader_.set(uLight1_, glm::normalize(glm::vec3(-0.45f, 0.75f, 0.55f)));
    shader_.set(uLight2_, glm::normalize(glm::vec3(0.6f, 0.2f, 0.4f)));
    if (!scene.bounds.isEmpty()) {
        drawGrid(scene, viewProjection);
    }
    shader_.set(uViewProj_, viewProjection);

    // 1. Shaded panels, pushed slightly back so that their outlines are drawn on top.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    shader_.set(uLit_, 1);
    shader_.set(uTile_, kTextureTile);
    for (const SceneBox& box : scene.boxes) {
        const glm::mat4 model = boxMatrix(box.min, box.max);
        shader_.set(uModel_, model);
        shader_.set(uNormalMatrix_, normalMatrix(model));
        shader_.set(uColor_, box.color);
        useSurface(box.surface);
        cube_.draw();
    }
    uploadPrisms(scene);
    shader_.set(uModel_, glm::mat4(1.0f));
    shader_.set(uNormalMatrix_, glm::mat3(1.0f));
    for (std::size_t i = 0; i < scene.prisms.size(); ++i) {
        shader_.set(uColor_, scene.prisms[i].color);
        useSurface(scene.prisms[i].surface);
        prismFaces_[i]->draw();
    }
    shader_.set(uTextured_, 0);

    // 2. Holes: dark discs just above the surfaces.
    shader_.set(uLit_, 0);
    shader_.set(uColor_, kHole);
    for (const SceneHole& hole : scene.holes) {
        shader_.set(uModel_, discMatrix(hole));
        disc_.draw();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);

    // 3. Outlines of every panel; the selected composite gets an orange frame.
    for (const SceneBox& box : scene.boxes) {
        shader_.set(uModel_, boxMatrix(box.min, box.max));
        shader_.set(uColor_, box.selected ? kSelectedOutline : kOutline);
        cubeEdges_.draw();
    }
    shader_.set(uModel_, glm::mat4(1.0f));
    for (std::size_t i = 0; i < scene.prisms.size(); ++i) {
        shader_.set(uColor_, scene.prisms[i].selected ? kSelectedOutline : kOutline);
        prismEdges_[i]->draw();
    }
    if (scene.highlight) {
        const glm::vec3 min(static_cast<float>(scene.highlight->min().x),
                            static_cast<float>(scene.highlight->min().y),
                            static_cast<float>(scene.highlight->min().z));
        const glm::vec3 max(static_cast<float>(scene.highlight->max().x),
                            static_cast<float>(scene.highlight->max().y),
                            static_cast<float>(scene.highlight->max().z));
        shader_.set(uModel_, boxMatrix(min - glm::vec3(1.0f), max + glm::vec3(1.0f)));
        shader_.set(uColor_, kSelectedOutline);
        glDisable(GL_DEPTH_TEST); // visible through the panels
        cubeEdges_.draw();
        glEnable(GL_DEPTH_TEST);
    }

    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
    target_.resolve();
    return target_.texture();
}

} // namespace rc::render
