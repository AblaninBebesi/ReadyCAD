#include "readycad/render/GlObjects.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc::render {

namespace {

GLuint compile(GLenum type, std::string_view source)
{
    const GLuint shader = glCreateShader(type);
    const char* text = source.data();
    const auto length = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &text, &length);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("GLSL compile error: " + log);
    }
    return shader;
}

} // namespace

ShaderProgram::ShaderProgram(std::string_view vertexSource, std::string_view fragmentSource)
{
    const GLuint vs = compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = 0;
    try {
        fs = compile(GL_FRAGMENT_SHADER, fragmentSource);
    } catch (...) {
        glDeleteShader(vs);
        throw;
    }

    program_ = GlName<ProgramDeleter>(glCreateProgram());
    glAttachShader(program_.get(), vs);
    glAttachShader(program_.get(), fs);
    glLinkProgram(program_.get());
    glDeleteShader(vs); // the program keeps what it needs
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program_.get(), GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint logLength = 0;
        glGetProgramiv(program_.get(), GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetProgramInfoLog(program_.get(), logLength, nullptr, log.data());
        throw std::runtime_error("GLSL link error: " + log);
    }
}

GLint ShaderProgram::location(const char* name) const noexcept
{
    return glGetUniformLocation(program_.get(), name);
}

void ShaderProgram::set(GLint location, const glm::mat4& value) const noexcept
{
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void ShaderProgram::set(GLint location, const glm::mat3& value) const noexcept
{
    glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void ShaderProgram::set(GLint location, const glm::vec3& value) const noexcept
{
    glUniform3fv(location, 1, glm::value_ptr(value));
}

void ShaderProgram::set(GLint location, const glm::vec4& value) const noexcept
{
    glUniform4fv(location, 1, glm::value_ptr(value));
}

void ShaderProgram::set(GLint location, int value) const noexcept
{
    glUniform1i(location, value);
}

Mesh::Mesh(const float* vertices, GLsizei vertexCount, GLenum primitive)
    : count_(vertexCount), primitive_(primitive)
{
    GLuint name = 0;
    glGenVertexArrays(1, &name);
    vao_ = GlVertexArray(name);
    glGenBuffers(1, &name);
    vbo_ = GlBuffer(name);

    glBindVertexArray(vao_.get());
    glBindBuffer(GL_ARRAY_BUFFER, vbo_.get());
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sizeof(float) * 6 * static_cast<std::size_t>(vertexCount)),
                 vertices, GL_STATIC_DRAW);
    constexpr GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Mesh::draw() const noexcept
{
    glBindVertexArray(vao_.get());
    glDrawArrays(primitive_, 0, count_);
}

void OffscreenTarget::resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == width_ && height == height_ && msaaFbo_) {
        return;
    }
    width_ = width;
    height_ = height;
    constexpr GLsizei kSamples = 4;

    // Old objects are released by the assignments (RAII).
    GLuint name = 0;
    glGenRenderbuffers(1, &name);
    msaaColor_ = GlRenderbuffer(name);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaColor_.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, kSamples, GL_RGBA8, width, height);

    glGenRenderbuffers(1, &name);
    msaaDepth_ = GlRenderbuffer(name);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth_.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, kSamples, GL_DEPTH_COMPONENT24, width, height);

    glGenFramebuffers(1, &name);
    msaaFbo_ = GlFramebuffer(name);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              msaaColor_.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              msaaDepth_.get());

    glGenTextures(1, &name);
    colorTexture_ = GlTexture(name);
    glBindTexture(GL_TEXTURE_2D, colorTexture_.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &name);
    resolveFbo_ = GlFramebuffer(name);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           colorTexture_.get(), 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Off-screen framebuffer is incomplete");
    }
}

void OffscreenTarget::bind() const noexcept
{
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_.get());
    glViewport(0, 0, width_, height_);
}

void OffscreenTarget::resolve() const noexcept
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo_.get());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_.get());
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace rc::render
