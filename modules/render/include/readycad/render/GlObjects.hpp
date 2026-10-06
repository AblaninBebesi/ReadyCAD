#pragma once

#include <glad/gl.h>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string_view>
#include <utility>

namespace rc::render {

/// Move-only owner of one OpenGL object name. `Deleter` frees it, e.g. glDeleteBuffers.
///
/// Every GPU resource of the program lives in one of these, so nothing is leaked on any path
/// (normal exit, exception, window resize) — the stage 10 "no leaks with OpenGL" requirement.
template <typename Deleter>
class GlName {
public:
    GlName() = default;
    explicit GlName(GLuint name) noexcept : name_(name) {}
    ~GlName() { reset(); }

    GlName(const GlName&) = delete;
    GlName& operator=(const GlName&) = delete;
    GlName(GlName&& other) noexcept : name_(std::exchange(other.name_, 0)) {}
    GlName& operator=(GlName&& other) noexcept
    {
        if (this != &other) {
            reset();
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }

    [[nodiscard]] GLuint get() const noexcept { return name_; }
    [[nodiscard]] explicit operator bool() const noexcept { return name_ != 0; }

    void reset() noexcept
    {
        if (name_ != 0) {
            Deleter{}(name_);
            name_ = 0;
        }
    }

private:
    GLuint name_ = 0;
};

struct BufferDeleter { void operator()(GLuint n) const noexcept { glDeleteBuffers(1, &n); } };
struct VertexArrayDeleter { void operator()(GLuint n) const noexcept { glDeleteVertexArrays(1, &n); } };
struct TextureDeleter { void operator()(GLuint n) const noexcept { glDeleteTextures(1, &n); } };
struct RenderbufferDeleter { void operator()(GLuint n) const noexcept { glDeleteRenderbuffers(1, &n); } };
struct FramebufferDeleter { void operator()(GLuint n) const noexcept { glDeleteFramebuffers(1, &n); } };
struct ProgramDeleter { void operator()(GLuint n) const noexcept { glDeleteProgram(n); } };

using GlBuffer = GlName<BufferDeleter>;
using GlVertexArray = GlName<VertexArrayDeleter>;
using GlTexture = GlName<TextureDeleter>;
using GlRenderbuffer = GlName<RenderbufferDeleter>;
using GlFramebuffer = GlName<FramebufferDeleter>;

/// Vertex + fragment shader program. Throws std::runtime_error with the GLSL log on failure.
class ShaderProgram {
public:
    ShaderProgram(std::string_view vertexSource, std::string_view fragmentSource);

    void use() const noexcept { glUseProgram(program_.get()); }
    [[nodiscard]] GLint location(const char* name) const noexcept;

    void set(GLint location, const glm::mat4& value) const noexcept;
    void set(GLint location, const glm::mat3& value) const noexcept;
    void set(GLint location, const glm::vec3& value) const noexcept;
    void set(GLint location, const glm::vec4& value) const noexcept;
    void set(GLint location, int value) const noexcept;

private:
    GlName<ProgramDeleter> program_;
};

/// Static triangle or line mesh with interleaved position + normal (6 floats per vertex).
class Mesh {
public:
    Mesh(const float* vertices, GLsizei vertexCount, GLenum primitive);
    void draw() const noexcept;

private:
    GlVertexArray vao_;
    GlBuffer vbo_;
    GLsizei count_;
    GLenum primitive_;
};

/// Off-screen target: draws with 4x MSAA, then resolves into a texture ImGui can show.
class OffscreenTarget {
public:
    void resize(int width, int height);
    void bind() const noexcept;
    /// Resolves the multisampled image into texture(); call after drawing.
    void resolve() const noexcept;

    [[nodiscard]] GLuint texture() const noexcept { return colorTexture_.get(); }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

private:
    int width_ = 0;
    int height_ = 0;
    GlFramebuffer msaaFbo_;
    GlRenderbuffer msaaColor_;
    GlRenderbuffer msaaDepth_;
    GlFramebuffer resolveFbo_;
    GlTexture colorTexture_;
};

} // namespace rc::render
