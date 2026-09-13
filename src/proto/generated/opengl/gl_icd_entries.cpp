// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opengl --xml gl.xml --wgl wgl.xml
//                  --profile tools/codegen/profiles/opengl.toml --out ...

#include "gl_icd_entries.h"

#include <cstdint>

#include "rvk/apis/opengl/icd/support.hpp"
#include "gl_dispatch_client.h"
#include "gl_helpers.h"

// Every entry point whose whole job is to forward.
extern "C" {

void APIENTRY glActiveShaderProgram(GLuint pipeline, GLuint program) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glActiveShaderProgram(*rvk_t, pipeline, program);
}

void APIENTRY glActiveTexture(GLenum texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    if (::rvk::glicd::skip_active_texture(texture)) return;
    ::av::glproto::rvk_glActiveTexture(*rvk_t, texture);
}

void APIENTRY glAttachShader(GLuint program, GLuint shader) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glAttachShader(*rvk_t, program, shader);
}

void APIENTRY glBeginConditionalRender(GLuint id, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBeginConditionalRender(*rvk_t, id, mode);
}

void APIENTRY glBeginQuery(GLenum target, GLuint id) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBeginQuery(*rvk_t, target, id);
}

void APIENTRY glBeginQueryIndexed(GLenum target, GLuint index, GLuint id) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBeginQueryIndexed(*rvk_t, target, index, id);
}

void APIENTRY glBeginTransformFeedback(GLenum primitiveMode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBeginTransformFeedback(*rvk_t, primitiveMode);
}

void APIENTRY glBindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindAttribLocation(*rvk_t, program, index, name);
}

void APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindBufferBase(*rvk_t, target, index, buffer);
}

void APIENTRY glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindBufferRange(*rvk_t, target, index, buffer, offset, size);
}

void APIENTRY glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint* buffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindBuffersBase(*rvk_t, target, first, count, buffers);
}

void APIENTRY glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizeiptr* sizes) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindBuffersRange(*rvk_t, target, first, count, buffers, offsets, sizes);
}

void APIENTRY glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindFragDataLocation(*rvk_t, program, color, name);
}

void APIENTRY glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindFragDataLocationIndexed(*rvk_t, program, colorNumber, index, name);
}

void APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindFramebuffer(*rvk_t, target, framebuffer);
}

void APIENTRY glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindImageTexture(*rvk_t, unit, texture, level, layered, layer, access, format);
}

void APIENTRY glBindImageTextures(GLuint first, GLsizei count, const GLuint* textures) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindImageTextures(*rvk_t, first, count, textures);
}

void APIENTRY glBindProgramPipeline(GLuint pipeline) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindProgramPipeline(*rvk_t, pipeline);
}

void APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindRenderbuffer(*rvk_t, target, renderbuffer);
}

void APIENTRY glBindSampler(GLuint unit, GLuint sampler) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    if (::rvk::glicd::skip_bind_sampler(unit, sampler)) return;
    ::av::glproto::rvk_glBindSampler(*rvk_t, unit, sampler);
}

void APIENTRY glBindSamplers(GLuint first, GLsizei count, const GLuint* samplers) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindSamplers(*rvk_t, first, count, samplers);
}

void APIENTRY glBindTexture(GLenum target, GLuint texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    if (::rvk::glicd::skip_bind_texture(target, texture)) return;
    ::av::glproto::rvk_glBindTexture(*rvk_t, target, texture);
}

void APIENTRY glBindTextureUnit(GLuint unit, GLuint texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindTextureUnit(*rvk_t, unit, texture);
}

void APIENTRY glBindTextures(GLuint first, GLsizei count, const GLuint* textures) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindTextures(*rvk_t, first, count, textures);
}

void APIENTRY glBindTransformFeedback(GLenum target, GLuint id) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindTransformFeedback(*rvk_t, target, id);
}

void APIENTRY glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindVertexBuffer(*rvk_t, bindingindex, buffer, offset, stride);
}

void APIENTRY glBindVertexBuffers(GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindVertexBuffers(*rvk_t, first, count, buffers, offsets, strides);
}

void APIENTRY glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendColor(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glBlendEquation(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendEquation(*rvk_t, mode);
}

void APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendEquationSeparate(*rvk_t, modeRGB, modeAlpha);
}

void APIENTRY glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendEquationSeparatei(*rvk_t, buf, modeRGB, modeAlpha);
}

void APIENTRY glBlendEquationi(GLuint buf, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendEquationi(*rvk_t, buf, mode);
}

void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendFunc(*rvk_t, sfactor, dfactor);
}

void APIENTRY glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendFuncSeparate(*rvk_t, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

void APIENTRY glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendFuncSeparatei(*rvk_t, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void APIENTRY glBlendFunci(GLuint buf, GLenum src, GLenum dst) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlendFunci(*rvk_t, buf, src, dst);
}

void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlitFramebuffer(*rvk_t, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void APIENTRY glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    auto* rvk_t = ::rvk::glicd::wire();
    readFramebuffer = ::rvk::glicd::framebuffer_name(readFramebuffer);
    drawFramebuffer = ::rvk::glicd::framebuffer_name(drawFramebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBlitNamedFramebuffer(*rvk_t, readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBufferSubData(*rvk_t, target, offset, size, data);
}

GLenum APIENTRY glCheckFramebufferStatus(GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glCheckFramebufferStatus(*rvk_t, target);
}

GLenum APIENTRY glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glCheckNamedFramebufferStatus(*rvk_t, framebuffer, target);
}

void APIENTRY glClampColor(GLenum target, GLenum clamp) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClampColor(*rvk_t, target, clamp);
}

void APIENTRY glClear(GLbitfield mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClear(*rvk_t, mask);
}

void APIENTRY glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearBufferData(*rvk_t, target, internalformat, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, 1, 1, 1, false), rvk_data_offset);
}

void APIENTRY glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearBufferSubData(*rvk_t, target, internalformat, offset, size, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, 1, 1, 1, false), rvk_data_offset);
}

void APIENTRY glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearBufferfi(*rvk_t, buffer, drawbuffer, depth, stencil);
}

void APIENTRY glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearBufferfv(*rvk_t, buffer, drawbuffer, value);
}

void APIENTRY glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearBufferiv(*rvk_t, buffer, drawbuffer, value);
}

void APIENTRY glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearBufferuiv(*rvk_t, buffer, drawbuffer, value);
}

void APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearColor(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glClearDepth(GLdouble depth) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearDepth(*rvk_t, depth);
}

void APIENTRY glClearDepthf(GLfloat d) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearDepthf(*rvk_t, d);
}

void APIENTRY glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearNamedBufferData(*rvk_t, buffer, internalformat, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, 1, 1, 1, false), rvk_data_offset);
}

void APIENTRY glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearNamedBufferSubData(*rvk_t, buffer, internalformat, offset, size, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, 1, 1, 1, false), rvk_data_offset);
}

void APIENTRY glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearNamedFramebufferfi(*rvk_t, framebuffer, buffer, drawbuffer, depth, stencil);
}

void APIENTRY glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearNamedFramebufferfv(*rvk_t, framebuffer, buffer, drawbuffer, value);
}

void APIENTRY glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearNamedFramebufferiv(*rvk_t, framebuffer, buffer, drawbuffer, value);
}

void APIENTRY glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearNamedFramebufferuiv(*rvk_t, framebuffer, buffer, drawbuffer, value);
}

void APIENTRY glClearStencil(GLint s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearStencil(*rvk_t, s);
}

void APIENTRY glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearTexImage(*rvk_t, texture, level, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, 1, 1, 1, false), rvk_data_offset);
}

void APIENTRY glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearTexSubImage(*rvk_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_data_offset);
}

void APIENTRY glClipControl(GLenum origin, GLenum depth) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClipControl(*rvk_t, origin, depth);
}

void APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorMask(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorMaski(*rvk_t, index, r, g, b, a);
}

void APIENTRY glColorP3ui(GLenum type, GLuint color) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorP3ui(*rvk_t, type, color);
}

void APIENTRY glColorP3uiv(GLenum type, const GLuint* color) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorP3uiv(*rvk_t, type, color);
}

void APIENTRY glColorP4ui(GLenum type, GLuint color) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorP4ui(*rvk_t, type, color);
}

void APIENTRY glColorP4uiv(GLenum type, const GLuint* color) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorP4uiv(*rvk_t, type, color);
}

void APIENTRY glCompileShader(GLuint shader) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompileShader(*rvk_t, shader);
}

void APIENTRY glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTexImage1D(*rvk_t, target, level, internalformat, width, border, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, 1, 1, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTexImage2D(*rvk_t, target, level, internalformat, width, height, border, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, height, 1, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTexImage3D(*rvk_t, target, level, internalformat, width, height, depth, border, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, height, depth, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTexSubImage1D(*rvk_t, target, level, xoffset, width, format, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, 1, 1, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTexSubImage2D(*rvk_t, target, level, xoffset, yoffset, width, height, format, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, height, 1, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTexSubImage3D(*rvk_t, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, height, depth, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTextureSubImage1D(*rvk_t, texture, level, xoffset, width, format, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, 1, 1, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTextureSubImage2D(*rvk_t, texture, level, xoffset, yoffset, width, height, format, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, height, 1, imageSize, false), rvk_data_offset);
}

void APIENTRY glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glCompressedTextureSubImage3D(*rvk_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data, rvk_data_offset ? 0 : ::rvk::glicd::compressed_bytes(width, height, depth, imageSize, false), rvk_data_offset);
}

void APIENTRY glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyBufferSubData(*rvk_t, readTarget, writeTarget, readOffset, writeOffset, size);
}

void APIENTRY glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyImageSubData(*rvk_t, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

void APIENTRY glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyNamedBufferSubData(*rvk_t, readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void APIENTRY glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTexImage1D(*rvk_t, target, level, internalformat, x, y, width, border);
}

void APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTexImage2D(*rvk_t, target, level, internalformat, x, y, width, height, border);
}

void APIENTRY glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTexSubImage1D(*rvk_t, target, level, xoffset, x, y, width);
}

void APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTexSubImage2D(*rvk_t, target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTexSubImage3D(*rvk_t, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureSubImage1D(*rvk_t, texture, level, xoffset, x, y, width);
}

void APIENTRY glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureSubImage2D(*rvk_t, texture, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureSubImage3D(*rvk_t, texture, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY glCreateBuffers(GLsizei n, GLuint* buffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateBuffers(*rvk_t, n, buffers);
}

void APIENTRY glCreateFramebuffers(GLsizei n, GLuint* framebuffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateFramebuffers(*rvk_t, n, framebuffers);
}

GLuint APIENTRY glCreateProgram(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glCreateProgram(*rvk_t);
}

void APIENTRY glCreateProgramPipelines(GLsizei n, GLuint* pipelines) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateProgramPipelines(*rvk_t, n, pipelines);
}

void APIENTRY glCreateQueries(GLenum target, GLsizei n, GLuint* ids) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateQueries(*rvk_t, target, n, ids);
}

void APIENTRY glCreateRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateRenderbuffers(*rvk_t, n, renderbuffers);
}

void APIENTRY glCreateSamplers(GLsizei n, GLuint* samplers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateSamplers(*rvk_t, n, samplers);
}

GLuint APIENTRY glCreateShader(GLenum type) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glCreateShader(*rvk_t, type);
}

GLuint APIENTRY glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar** strings) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glCreateShaderProgramv(*rvk_t, type, count, strings);
}

void APIENTRY glCreateTextures(GLenum target, GLsizei n, GLuint* textures) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateTextures(*rvk_t, target, n, textures);
}

void APIENTRY glCreateTransformFeedbacks(GLsizei n, GLuint* ids) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateTransformFeedbacks(*rvk_t, n, ids);
}

void APIENTRY glCreateVertexArrays(GLsizei n, GLuint* arrays) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCreateVertexArrays(*rvk_t, n, arrays);
}

void APIENTRY glCullFace(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCullFace(*rvk_t, mode);
}

void APIENTRY glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDebugMessageControl(*rvk_t, source, type, severity, count, ids, enabled);
}

void APIENTRY glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDebugMessageInsert(*rvk_t, source, type, id, severity, length, buf);
}

void APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteFramebuffers(*rvk_t, n, framebuffers);
}

void APIENTRY glDeleteProgramPipelines(GLsizei n, const GLuint* pipelines) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteProgramPipelines(*rvk_t, n, pipelines);
}

void APIENTRY glDeleteQueries(GLsizei n, const GLuint* ids) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteQueries(*rvk_t, n, ids);
}

void APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteRenderbuffers(*rvk_t, n, renderbuffers);
}

void APIENTRY glDeleteSamplers(GLsizei count, const GLuint* samplers) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteSamplers(*rvk_t, count, samplers);
}

void APIENTRY glDeleteShader(GLuint shader) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteShader(*rvk_t, shader);
}

void APIENTRY glDeleteSync(GLsync sync) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteSync(*rvk_t, reinterpret_cast<std::uint64_t>(sync));
}

void APIENTRY glDeleteTextures(GLsizei n, const GLuint* textures) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteTextures(*rvk_t, n, textures);
}

void APIENTRY glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteTransformFeedbacks(*rvk_t, n, ids);
}

void APIENTRY glDepthFunc(GLenum func) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDepthFunc(*rvk_t, func);
}

void APIENTRY glDepthMask(GLboolean flag) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDepthMask(*rvk_t, flag);
}

void APIENTRY glDepthRange(GLdouble n, GLdouble f) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDepthRange(*rvk_t, n, f);
}

void APIENTRY glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDepthRangeArrayv(*rvk_t, first, count, v);
}

void APIENTRY glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDepthRangeIndexed(*rvk_t, index, n, f);
}

void APIENTRY glDepthRangef(GLfloat n, GLfloat f) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDepthRangef(*rvk_t, n, f);
}

void APIENTRY glDetachShader(GLuint program, GLuint shader) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDetachShader(*rvk_t, program, shader);
}

void APIENTRY glDisable(GLenum cap) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisable(*rvk_t, cap);
}

void APIENTRY glDisableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisableVertexArrayAttrib(*rvk_t, vaobj, index);
}

void APIENTRY glDisablei(GLenum target, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisablei(*rvk_t, target, index);
}

void APIENTRY glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDispatchCompute(*rvk_t, num_groups_x, num_groups_y, num_groups_z);
}

void APIENTRY glDispatchComputeIndirect(GLintptr indirect) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDispatchComputeIndirect(*rvk_t, indirect);
}

void APIENTRY glDrawArraysIndirect(GLenum mode, const void* indirect) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawArraysIndirect(*rvk_t, mode, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indirect)));
}

void APIENTRY glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawArraysInstancedBaseInstance(*rvk_t, mode, first, count, instancecount, baseinstance);
}

void APIENTRY glDrawBuffer(GLenum buf) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawBuffer(*rvk_t, buf);
}

void APIENTRY glDrawBuffers(GLsizei n, const GLenum* bufs) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawBuffers(*rvk_t, n, bufs);
}

void APIENTRY glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawElementsBaseVertex(*rvk_t, mode, count, type, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indices)), basevertex);
}

void APIENTRY glDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawElementsIndirect(*rvk_t, mode, type, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indirect)));
}

void APIENTRY glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLuint baseinstance) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawElementsInstancedBaseInstance(*rvk_t, mode, count, type, indices, instancecount, baseinstance);
}

void APIENTRY glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawElementsInstancedBaseVertex(*rvk_t, mode, count, type, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indices)), instancecount, basevertex);
}

void APIENTRY glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawElementsInstancedBaseVertexBaseInstance(*rvk_t, mode, count, type, indices, instancecount, basevertex, baseinstance);
}

void APIENTRY glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices, GLint basevertex) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawRangeElementsBaseVertex(*rvk_t, mode, start, end, count, type, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indices)), basevertex);
}

void APIENTRY glDrawTransformFeedback(GLenum mode, GLuint id) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawTransformFeedback(*rvk_t, mode, id);
}

void APIENTRY glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawTransformFeedbackInstanced(*rvk_t, mode, id, instancecount);
}

void APIENTRY glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawTransformFeedbackStream(*rvk_t, mode, id, stream);
}

void APIENTRY glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDrawTransformFeedbackStreamInstanced(*rvk_t, mode, id, stream, instancecount);
}

void APIENTRY glEnable(GLenum cap) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnable(*rvk_t, cap);
}

void APIENTRY glEnableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnableVertexArrayAttrib(*rvk_t, vaobj, index);
}

void APIENTRY glEnablei(GLenum target, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnablei(*rvk_t, target, index);
}

void APIENTRY glEndConditionalRender(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEndConditionalRender(*rvk_t);
}

void APIENTRY glEndQuery(GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEndQuery(*rvk_t, target);
}

void APIENTRY glEndQueryIndexed(GLenum target, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEndQueryIndexed(*rvk_t, target, index);
}

void APIENTRY glEndTransformFeedback(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEndTransformFeedback(*rvk_t);
}

GLsync APIENTRY glFenceSync(GLenum condition, GLbitfield flags) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return nullptr;
    return reinterpret_cast<GLsync>(static_cast<std::uintptr_t>(::av::glproto::rvk_glFenceSync(*rvk_t, condition, flags)));
}

void APIENTRY glFinish(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFinish(*rvk_t);
}

void APIENTRY glFramebufferParameteri(GLenum target, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferParameteri(*rvk_t, target, pname, param);
}

void APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferRenderbuffer(*rvk_t, target, attachment, renderbuffertarget, renderbuffer);
}

void APIENTRY glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferTexture(*rvk_t, target, attachment, texture, level);
}

void APIENTRY glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferTexture1D(*rvk_t, target, attachment, textarget, texture, level);
}

void APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferTexture2D(*rvk_t, target, attachment, textarget, texture, level);
}

void APIENTRY glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferTexture3D(*rvk_t, target, attachment, textarget, texture, level, zoffset);
}

void APIENTRY glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferTextureLayer(*rvk_t, target, attachment, texture, level, layer);
}

void APIENTRY glFrontFace(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFrontFace(*rvk_t, mode);
}

void APIENTRY glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenFramebuffers(*rvk_t, n, framebuffers);
}

void APIENTRY glGenProgramPipelines(GLsizei n, GLuint* pipelines) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenProgramPipelines(*rvk_t, n, pipelines);
}

void APIENTRY glGenQueries(GLsizei n, GLuint* ids) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenQueries(*rvk_t, n, ids);
}

void APIENTRY glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenRenderbuffers(*rvk_t, n, renderbuffers);
}

void APIENTRY glGenSamplers(GLsizei count, GLuint* samplers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenSamplers(*rvk_t, count, samplers);
}

void APIENTRY glGenTransformFeedbacks(GLsizei n, GLuint* ids) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenTransformFeedbacks(*rvk_t, n, ids);
}

void APIENTRY glGenerateMipmap(GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenerateMipmap(*rvk_t, target);
}

void APIENTRY glGenerateTextureMipmap(GLuint texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenerateTextureMipmap(*rvk_t, texture);
}

void APIENTRY glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveAtomicCounterBufferiv(*rvk_t, program, bufferIndex, pname, params);
}

void APIENTRY glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveAttrib(*rvk_t, program, index, bufSize, length, size, type, name);
}

void APIENTRY glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveSubroutineName(*rvk_t, program, shadertype, index, bufSize, length, name);
}

void APIENTRY glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveSubroutineUniformName(*rvk_t, program, shadertype, index, bufSize, length, name);
}

void APIENTRY glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveSubroutineUniformiv(*rvk_t, program, shadertype, index, pname, values);
}

void APIENTRY glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveUniform(*rvk_t, program, index, bufSize, length, size, type, name);
}

void APIENTRY glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveUniformBlockName(*rvk_t, program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void APIENTRY glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveUniformBlockiv(*rvk_t, program, uniformBlockIndex, pname, params);
}

void APIENTRY glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveUniformName(*rvk_t, program, uniformIndex, bufSize, length, uniformName);
}

void APIENTRY glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetActiveUniformsiv(*rvk_t, program, uniformCount, uniformIndices, pname, params);
}

void APIENTRY glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetAttachedShaders(*rvk_t, program, maxCount, count, shaders);
}

void APIENTRY glGetBooleani_v(GLenum target, GLuint index, GLboolean* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetBooleani_v(*rvk_t, target, index, data);
}

void APIENTRY glGetBooleanv(GLenum pname, GLboolean* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetBooleanv(*rvk_t, pname, data);
}

void APIENTRY glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetBufferParameteri64v(*rvk_t, target, pname, params);
}

void APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetBufferParameteriv(*rvk_t, target, pname, params);
}

void APIENTRY glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetBufferSubData(*rvk_t, target, offset, size, data);
}

void APIENTRY glGetCompressedTexImage(GLenum target, GLint level, void* img) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_img_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetCompressedTexImage(*rvk_t, target, level, img, rvk_img_offset ? 0 : ::rvk::glicd::compressed_level_bytes(target, level, true), rvk_img_offset);
}

void APIENTRY glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetCompressedTextureImage(*rvk_t, texture, level, bufSize, pixels, rvk_pixels_offset);
}

void APIENTRY glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetCompressedTextureSubImage(*rvk_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, bufSize, pixels, rvk_pixels_offset);
}

GLuint APIENTRY glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetDebugMessageLog(*rvk_t, count, bufSize, sources, types, ids, severities, lengths, messageLog);
}

void APIENTRY glGetDoublei_v(GLenum target, GLuint index, GLdouble* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetDoublei_v(*rvk_t, target, index, data);
}

void APIENTRY glGetDoublev(GLenum pname, GLdouble* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetDoublev(*rvk_t, pname, data);
}

GLenum APIENTRY glGetError(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetError(*rvk_t);
}

void APIENTRY glGetFloati_v(GLenum target, GLuint index, GLfloat* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetFloati_v(*rvk_t, target, index, data);
}

void APIENTRY glGetFloatv(GLenum pname, GLfloat* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetFloatv(*rvk_t, pname, data);
}

GLint APIENTRY glGetFragDataIndex(GLuint program, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetFragDataIndex(*rvk_t, program, name);
}

GLint APIENTRY glGetFragDataLocation(GLuint program, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetFragDataLocation(*rvk_t, program, name);
}

void APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetFramebufferAttachmentParameteriv(*rvk_t, target, attachment, pname, params);
}

void APIENTRY glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetFramebufferParameteriv(*rvk_t, target, pname, params);
}

GLenum APIENTRY glGetGraphicsResetStatus(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetGraphicsResetStatus(*rvk_t);
}

void APIENTRY glGetInteger64i_v(GLenum target, GLuint index, GLint64* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetInteger64i_v(*rvk_t, target, index, data);
}

void APIENTRY glGetInteger64v(GLenum pname, GLint64* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetInteger64v(*rvk_t, pname, data);
}

void APIENTRY glGetIntegeri_v(GLenum target, GLuint index, GLint* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetIntegeri_v(*rvk_t, target, index, data);
}

void APIENTRY glGetIntegerv(GLenum pname, GLint* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetIntegerv(*rvk_t, pname, data);
}

void APIENTRY glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint64* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetInternalformati64v(*rvk_t, target, internalformat, pname, count, params);
}

void APIENTRY glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetInternalformativ(*rvk_t, target, internalformat, pname, count, params);
}

void APIENTRY glGetMultisamplefv(GLenum pname, GLuint index, GLfloat* val) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultisamplefv(*rvk_t, pname, index, val);
}

void APIENTRY glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedBufferParameteri64v(*rvk_t, buffer, pname, params);
}

void APIENTRY glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedBufferParameteriv(*rvk_t, buffer, pname, params);
}

void APIENTRY glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedBufferSubData(*rvk_t, buffer, offset, size, data);
}

void APIENTRY glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedFramebufferAttachmentParameteriv(*rvk_t, framebuffer, attachment, pname, params);
}

void APIENTRY glGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname, GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedFramebufferParameteriv(*rvk_t, framebuffer, pname, param);
}

void APIENTRY glGetNamedRenderbufferParameteriv(GLuint renderbuffer, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedRenderbufferParameteriv(*rvk_t, renderbuffer, pname, params);
}

void APIENTRY glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetObjectLabel(*rvk_t, identifier, name, bufSize, length, label);
}

void APIENTRY glGetObjectPtrLabel(GLsync ptr, GLsizei bufSize, GLsizei* length, GLchar* label) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetObjectPtrLabel(*rvk_t, reinterpret_cast<std::uint64_t>(ptr), bufSize, length, label);
}

void APIENTRY glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramBinary(*rvk_t, program, bufSize, length, binaryFormat, binary);
}

void APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramInfoLog(*rvk_t, program, bufSize, length, infoLog);
}

void APIENTRY glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramInterfaceiv(*rvk_t, program, programInterface, pname, params);
}

void APIENTRY glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramPipelineInfoLog(*rvk_t, pipeline, bufSize, length, infoLog);
}

void APIENTRY glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramPipelineiv(*rvk_t, pipeline, pname, params);
}

GLuint APIENTRY glGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetProgramResourceIndex(*rvk_t, program, programInterface, name);
}

GLint APIENTRY glGetProgramResourceLocation(GLuint program, GLenum programInterface, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetProgramResourceLocation(*rvk_t, program, programInterface, name);
}

GLint APIENTRY glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetProgramResourceLocationIndex(*rvk_t, program, programInterface, name);
}

void APIENTRY glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramResourceName(*rvk_t, program, programInterface, index, bufSize, length, name);
}

void APIENTRY glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum* props, GLsizei count, GLsizei* length, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramResourceiv(*rvk_t, program, programInterface, index, propCount, props, count, length, params);
}

void APIENTRY glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramStageiv(*rvk_t, program, shadertype, pname, values);
}

void APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetProgramiv(*rvk_t, program, pname, params);
}

void APIENTRY glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetQueryBufferObjecti64v(*rvk_t, id, buffer, pname, offset);
}

void APIENTRY glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetQueryBufferObjectiv(*rvk_t, id, buffer, pname, offset);
}

void APIENTRY glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetQueryBufferObjectui64v(*rvk_t, id, buffer, pname, offset);
}

void APIENTRY glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetQueryBufferObjectuiv(*rvk_t, id, buffer, pname, offset);
}

void APIENTRY glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetQueryIndexediv(*rvk_t, target, index, pname, params);
}

void APIENTRY glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_params_offset = ::rvk::glicd::query_buffer_bound();
    ::av::glproto::rvk_glGetQueryObjecti64v(*rvk_t, id, pname, params, rvk_params_offset ? 0 : ::av::glproto::pname_count(*rvk_t, pname) * sizeof(GLint64), rvk_params_offset);
}

void APIENTRY glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_params_offset = ::rvk::glicd::query_buffer_bound();
    ::av::glproto::rvk_glGetQueryObjectiv(*rvk_t, id, pname, params, rvk_params_offset ? 0 : ::av::glproto::pname_count(*rvk_t, pname) * sizeof(GLint), rvk_params_offset);
}

void APIENTRY glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_params_offset = ::rvk::glicd::query_buffer_bound();
    ::av::glproto::rvk_glGetQueryObjectui64v(*rvk_t, id, pname, params, rvk_params_offset ? 0 : ::av::glproto::pname_count(*rvk_t, pname) * sizeof(GLuint64), rvk_params_offset);
}

void APIENTRY glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_params_offset = ::rvk::glicd::query_buffer_bound();
    ::av::glproto::rvk_glGetQueryObjectuiv(*rvk_t, id, pname, params, rvk_params_offset ? 0 : ::av::glproto::pname_count(*rvk_t, pname) * sizeof(GLuint), rvk_params_offset);
}

void APIENTRY glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetQueryiv(*rvk_t, target, pname, params);
}

void APIENTRY glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetRenderbufferParameteriv(*rvk_t, target, pname, params);
}

void APIENTRY glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetSamplerParameterIiv(*rvk_t, sampler, pname, params);
}

void APIENTRY glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetSamplerParameterIuiv(*rvk_t, sampler, pname, params);
}

void APIENTRY glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetSamplerParameterfv(*rvk_t, sampler, pname, params);
}

void APIENTRY glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetSamplerParameteriv(*rvk_t, sampler, pname, params);
}

void APIENTRY glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetShaderInfoLog(*rvk_t, shader, bufSize, length, infoLog);
}

void APIENTRY glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetShaderPrecisionFormat(*rvk_t, shadertype, precisiontype, range, precision);
}

void APIENTRY glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetShaderSource(*rvk_t, shader, bufSize, length, source);
}

void APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetShaderiv(*rvk_t, shader, pname, params);
}

GLuint APIENTRY glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetSubroutineIndex(*rvk_t, program, shadertype, name);
}

GLint APIENTRY glGetSubroutineUniformLocation(GLuint program, GLenum shadertype, const GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetSubroutineUniformLocation(*rvk_t, program, shadertype, name);
}

void APIENTRY glGetSynciv(GLsync sync, GLenum pname, GLsizei count, GLsizei* length, GLint* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetSynciv(*rvk_t, reinterpret_cast<std::uint64_t>(sync), pname, count, length, values);
}

void APIENTRY glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetTexImage(*rvk_t, target, level, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::level_bytes(target, level, format, type), rvk_pixels_offset);
}

void APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexLevelParameterfv(*rvk_t, target, level, pname, params);
}

void APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexLevelParameteriv(*rvk_t, target, level, pname, params);
}

void APIENTRY glGetTexParameterIiv(GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexParameterIiv(*rvk_t, target, pname, params);
}

void APIENTRY glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexParameterIuiv(*rvk_t, target, pname, params);
}

void APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexParameterfv(*rvk_t, target, pname, params);
}

void APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexParameteriv(*rvk_t, target, pname, params);
}

void APIENTRY glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetTextureImage(*rvk_t, texture, level, format, type, bufSize, pixels, rvk_pixels_offset);
}

void APIENTRY glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureLevelParameterfv(*rvk_t, texture, level, pname, params);
}

void APIENTRY glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureLevelParameteriv(*rvk_t, texture, level, pname, params);
}

void APIENTRY glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameterIiv(*rvk_t, texture, pname, params);
}

void APIENTRY glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameterIuiv(*rvk_t, texture, pname, params);
}

void APIENTRY glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameterfv(*rvk_t, texture, pname, params);
}

void APIENTRY glGetTextureParameteriv(GLuint texture, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameteriv(*rvk_t, texture, pname, params);
}

void APIENTRY glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetTextureSubImage(*rvk_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, bufSize, pixels, rvk_pixels_offset);
}

void APIENTRY glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTransformFeedbackVarying(*rvk_t, program, index, bufSize, length, size, type, name);
}

void APIENTRY glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTransformFeedbacki64_v(*rvk_t, xfb, pname, index, param);
}

void APIENTRY glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTransformFeedbacki_v(*rvk_t, xfb, pname, index, param);
}

void APIENTRY glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTransformFeedbackiv(*rvk_t, xfb, pname, param);
}

GLuint APIENTRY glGetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetUniformBlockIndex(*rvk_t, program, uniformBlockName);
}

void APIENTRY glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar** uniformNames, GLuint* uniformIndices) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformIndices(*rvk_t, program, uniformCount, uniformNames, uniformIndices);
}

void APIENTRY glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformSubroutineuiv(*rvk_t, shadertype, location, params);
}

void APIENTRY glGetUniformdv(GLuint program, GLint location, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformdv(*rvk_t, program, location, params);
}

void APIENTRY glGetUniformfv(GLuint program, GLint location, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformfv(*rvk_t, program, location, params);
}

void APIENTRY glGetUniformiv(GLuint program, GLint location, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformiv(*rvk_t, program, location, params);
}

void APIENTRY glGetUniformuiv(GLuint program, GLint location, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformuiv(*rvk_t, program, location, params);
}

void APIENTRY glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname, GLint64* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexArrayIndexed64iv(*rvk_t, vaobj, index, pname, param);
}

void APIENTRY glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexArrayIndexediv(*rvk_t, vaobj, index, pname, param);
}

void APIENTRY glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexArrayiv(*rvk_t, vaobj, pname, param);
}

void APIENTRY glGetVertexAttribIiv(GLuint index, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexAttribIiv(*rvk_t, index, pname, params);
}

void APIENTRY glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexAttribIuiv(*rvk_t, index, pname, params);
}

void APIENTRY glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexAttribLdv(*rvk_t, index, pname, params);
}

void APIENTRY glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexAttribdv(*rvk_t, index, pname, params);
}

void APIENTRY glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexAttribfv(*rvk_t, index, pname, params);
}

void APIENTRY glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexAttribiv(*rvk_t, index, pname, params);
}

void APIENTRY glGetnCompressedTexImage(GLenum target, GLint lod, GLsizei bufSize, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetnCompressedTexImage(*rvk_t, target, lod, bufSize, pixels, rvk_pixels_offset);
}

void APIENTRY glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetnTexImage(*rvk_t, target, level, format, type, bufSize, pixels, rvk_pixels_offset);
}

void APIENTRY glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetnUniformdv(*rvk_t, program, location, bufSize, params);
}

void APIENTRY glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetnUniformfv(*rvk_t, program, location, bufSize, params);
}

void APIENTRY glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetnUniformiv(*rvk_t, program, location, bufSize, params);
}

void APIENTRY glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetnUniformuiv(*rvk_t, program, location, bufSize, params);
}

void APIENTRY glHint(GLenum target, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glHint(*rvk_t, target, mode);
}

void APIENTRY glInvalidateBufferData(GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateBufferData(*rvk_t, buffer);
}

void APIENTRY glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateBufferSubData(*rvk_t, buffer, offset, length);
}

void APIENTRY glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateFramebuffer(*rvk_t, target, numAttachments, attachments);
}

void APIENTRY glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateNamedFramebufferData(*rvk_t, framebuffer, numAttachments, attachments);
}

void APIENTRY glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateNamedFramebufferSubData(*rvk_t, framebuffer, numAttachments, attachments, x, y, width, height);
}

void APIENTRY glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateSubFramebuffer(*rvk_t, target, numAttachments, attachments, x, y, width, height);
}

void APIENTRY glInvalidateTexImage(GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateTexImage(*rvk_t, texture, level);
}

void APIENTRY glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInvalidateTexSubImage(*rvk_t, texture, level, xoffset, yoffset, zoffset, width, height, depth);
}

GLboolean APIENTRY glIsBuffer(GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsBuffer(*rvk_t, buffer);
}

GLboolean APIENTRY glIsEnabled(GLenum cap) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsEnabled(*rvk_t, cap);
}

GLboolean APIENTRY glIsEnabledi(GLenum target, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsEnabledi(*rvk_t, target, index);
}

GLboolean APIENTRY glIsFramebuffer(GLuint framebuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsFramebuffer(*rvk_t, framebuffer);
}

GLboolean APIENTRY glIsProgram(GLuint program) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsProgram(*rvk_t, program);
}

GLboolean APIENTRY glIsProgramPipeline(GLuint pipeline) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsProgramPipeline(*rvk_t, pipeline);
}

GLboolean APIENTRY glIsQuery(GLuint id) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsQuery(*rvk_t, id);
}

GLboolean APIENTRY glIsRenderbuffer(GLuint renderbuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsRenderbuffer(*rvk_t, renderbuffer);
}

GLboolean APIENTRY glIsSampler(GLuint sampler) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsSampler(*rvk_t, sampler);
}

GLboolean APIENTRY glIsShader(GLuint shader) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsShader(*rvk_t, shader);
}

GLboolean APIENTRY glIsSync(GLsync sync) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsSync(*rvk_t, reinterpret_cast<std::uint64_t>(sync));
}

GLboolean APIENTRY glIsTexture(GLuint texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsTexture(*rvk_t, texture);
}

GLboolean APIENTRY glIsTransformFeedback(GLuint id) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsTransformFeedback(*rvk_t, id);
}

GLboolean APIENTRY glIsVertexArray(GLuint array) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsVertexArray(*rvk_t, array);
}

void APIENTRY glLineWidth(GLfloat width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLineWidth(*rvk_t, width);
}

void APIENTRY glLogicOp(GLenum opcode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLogicOp(*rvk_t, opcode);
}

void APIENTRY glMemoryBarrier(GLbitfield barriers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMemoryBarrier(*rvk_t, barriers);
}

void APIENTRY glMemoryBarrierByRegion(GLbitfield barriers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMemoryBarrierByRegion(*rvk_t, barriers);
}

void APIENTRY glMinSampleShading(GLfloat value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMinSampleShading(*rvk_t, value);
}

void APIENTRY glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiDrawArrays(*rvk_t, mode, first, count, drawcount);
}

void APIENTRY glMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiDrawArraysIndirect(*rvk_t, mode, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indirect)), drawcount, stride);
}

void APIENTRY glMultiDrawArraysIndirectCount(GLenum mode, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiDrawArraysIndirectCount(*rvk_t, mode, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indirect)), drawcount, maxdrawcount, stride);
}

void APIENTRY glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiDrawElements(*rvk_t, mode, count, type, ::rvk::glicd::offsets_of(indices, static_cast<std::size_t>(drawcount)), drawcount);
}

void APIENTRY glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount, const GLint* basevertex) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiDrawElementsBaseVertex(*rvk_t, mode, count, type, ::rvk::glicd::offsets_of(indices, static_cast<std::size_t>(drawcount)), drawcount, basevertex);
}

void APIENTRY glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiDrawElementsIndirect(*rvk_t, mode, type, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indirect)), drawcount, stride);
}

void APIENTRY glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiDrawElementsIndirectCount(*rvk_t, mode, type, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(indirect)), drawcount, maxdrawcount, stride);
}

void APIENTRY glMultiTexCoordP1ui(GLenum texture, GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP1ui(*rvk_t, texture, type, coords);
}

void APIENTRY glMultiTexCoordP1uiv(GLenum texture, GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP1uiv(*rvk_t, texture, type, coords);
}

void APIENTRY glMultiTexCoordP2ui(GLenum texture, GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP2ui(*rvk_t, texture, type, coords);
}

void APIENTRY glMultiTexCoordP2uiv(GLenum texture, GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP2uiv(*rvk_t, texture, type, coords);
}

void APIENTRY glMultiTexCoordP3ui(GLenum texture, GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP3ui(*rvk_t, texture, type, coords);
}

void APIENTRY glMultiTexCoordP3uiv(GLenum texture, GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP3uiv(*rvk_t, texture, type, coords);
}

void APIENTRY glMultiTexCoordP4ui(GLenum texture, GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP4ui(*rvk_t, texture, type, coords);
}

void APIENTRY glMultiTexCoordP4uiv(GLenum texture, GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordP4uiv(*rvk_t, texture, type, coords);
}

void APIENTRY glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedBufferSubData(*rvk_t, buffer, offset, size, data);
}

void APIENTRY glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum buf) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferDrawBuffer(*rvk_t, framebuffer, buf);
}

void APIENTRY glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferDrawBuffers(*rvk_t, framebuffer, n, bufs);
}

void APIENTRY glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferParameteri(*rvk_t, framebuffer, pname, param);
}

void APIENTRY glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum src) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferReadBuffer(*rvk_t, framebuffer, src);
}

void APIENTRY glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferRenderbuffer(*rvk_t, framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void APIENTRY glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTexture(*rvk_t, framebuffer, attachment, texture, level);
}

void APIENTRY glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTextureLayer(*rvk_t, framebuffer, attachment, texture, level, layer);
}

void APIENTRY glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedRenderbufferStorage(*rvk_t, renderbuffer, internalformat, width, height);
}

void APIENTRY glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedRenderbufferStorageMultisample(*rvk_t, renderbuffer, samples, internalformat, width, height);
}

void APIENTRY glNormalP3ui(GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormalP3ui(*rvk_t, type, coords);
}

void APIENTRY glNormalP3uiv(GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormalP3uiv(*rvk_t, type, coords);
}

void APIENTRY glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glObjectLabel(*rvk_t, identifier, name, length, label);
}

void APIENTRY glObjectPtrLabel(GLsync ptr, GLsizei length, const GLchar* label) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glObjectPtrLabel(*rvk_t, reinterpret_cast<std::uint64_t>(ptr), length, label);
}

void APIENTRY glPatchParameterfv(GLenum pname, const GLfloat* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPatchParameterfv(*rvk_t, pname, values);
}

void APIENTRY glPatchParameteri(GLenum pname, GLint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPatchParameteri(*rvk_t, pname, value);
}

void APIENTRY glPauseTransformFeedback(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPauseTransformFeedback(*rvk_t);
}

void APIENTRY glPointParameterf(GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPointParameterf(*rvk_t, pname, param);
}

void APIENTRY glPointParameterfv(GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPointParameterfv(*rvk_t, pname, params);
}

void APIENTRY glPointParameteri(GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPointParameteri(*rvk_t, pname, param);
}

void APIENTRY glPointParameteriv(GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPointParameteriv(*rvk_t, pname, params);
}

void APIENTRY glPointSize(GLfloat size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPointSize(*rvk_t, size);
}

void APIENTRY glPolygonMode(GLenum face, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPolygonMode(*rvk_t, face, mode);
}

void APIENTRY glPolygonOffset(GLfloat factor, GLfloat units) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPolygonOffset(*rvk_t, factor, units);
}

void APIENTRY glPolygonOffsetClamp(GLfloat factor, GLfloat units, GLfloat clamp) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPolygonOffsetClamp(*rvk_t, factor, units, clamp);
}

void APIENTRY glPopDebugGroup(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPopDebugGroup(*rvk_t);
}

void APIENTRY glPrimitiveRestartIndex(GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPrimitiveRestartIndex(*rvk_t, index);
}

void APIENTRY glProgramBinary(GLuint program, GLenum binaryFormat, const void* binary, GLsizei length) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramBinary(*rvk_t, program, binaryFormat, binary, length);
}

void APIENTRY glProgramParameteri(GLuint program, GLenum pname, GLint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramParameteri(*rvk_t, program, pname, value);
}

void APIENTRY glProgramUniform1d(GLuint program, GLint location, GLdouble v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1d(*rvk_t, program, location, v0);
}

void APIENTRY glProgramUniform1dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1dv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform1f(GLuint program, GLint location, GLfloat v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1f(*rvk_t, program, location, v0);
}

void APIENTRY glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1fv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform1i(GLuint program, GLint location, GLint v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1i(*rvk_t, program, location, v0);
}

void APIENTRY glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1iv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform1ui(GLuint program, GLint location, GLuint v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1ui(*rvk_t, program, location, v0);
}

void APIENTRY glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1uiv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2d(GLuint program, GLint location, GLdouble v0, GLdouble v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2d(*rvk_t, program, location, v0, v1);
}

void APIENTRY glProgramUniform2dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2dv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2f(*rvk_t, program, location, v0, v1);
}

void APIENTRY glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2fv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2i(*rvk_t, program, location, v0, v1);
}

void APIENTRY glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2iv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2ui(*rvk_t, program, location, v0, v1);
}

void APIENTRY glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2uiv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3d(*rvk_t, program, location, v0, v1, v2);
}

void APIENTRY glProgramUniform3dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3dv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3f(*rvk_t, program, location, v0, v1, v2);
}

void APIENTRY glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3fv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3i(*rvk_t, program, location, v0, v1, v2);
}

void APIENTRY glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3iv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3ui(*rvk_t, program, location, v0, v1, v2);
}

void APIENTRY glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3uiv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4d(*rvk_t, program, location, v0, v1, v2, v3);
}

void APIENTRY glProgramUniform4dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4dv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4f(*rvk_t, program, location, v0, v1, v2, v3);
}

void APIENTRY glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4fv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4i(*rvk_t, program, location, v0, v1, v2, v3);
}

void APIENTRY glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4iv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4ui(*rvk_t, program, location, v0, v1, v2, v3);
}

void APIENTRY glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4uiv(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniformMatrix2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x3dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x3fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x4dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x4fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x2dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x2fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x4dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x4fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x2dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x2fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x3dv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x3fv(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProvokingVertex(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProvokingVertex(*rvk_t, mode);
}

void APIENTRY glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPushDebugGroup(*rvk_t, source, id, length, message);
}

void APIENTRY glQueryCounter(GLuint id, GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glQueryCounter(*rvk_t, id, target);
}

void APIENTRY glReadBuffer(GLenum src) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glReadBuffer(*rvk_t, src);
}

void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glReadPixels(*rvk_t, x, y, width, height, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, true), rvk_pixels_offset);
}

void APIENTRY glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glReadnPixels(*rvk_t, x, y, width, height, format, type, bufSize, data, rvk_data_offset);
}

void APIENTRY glReleaseShaderCompiler(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glReleaseShaderCompiler(*rvk_t);
}

void APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRenderbufferStorage(*rvk_t, target, internalformat, width, height);
}

void APIENTRY glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRenderbufferStorageMultisample(*rvk_t, target, samples, internalformat, width, height);
}

void APIENTRY glResumeTransformFeedback(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glResumeTransformFeedback(*rvk_t);
}

void APIENTRY glSampleCoverage(GLfloat value, GLboolean invert) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSampleCoverage(*rvk_t, value, invert);
}

void APIENTRY glSampleMaski(GLuint maskNumber, GLbitfield mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSampleMaski(*rvk_t, maskNumber, mask);
}

void APIENTRY glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSamplerParameterIiv(*rvk_t, sampler, pname, param);
}

void APIENTRY glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSamplerParameterIuiv(*rvk_t, sampler, pname, param);
}

void APIENTRY glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSamplerParameterf(*rvk_t, sampler, pname, param);
}

void APIENTRY glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSamplerParameterfv(*rvk_t, sampler, pname, param);
}

void APIENTRY glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSamplerParameteri(*rvk_t, sampler, pname, param);
}

void APIENTRY glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSamplerParameteriv(*rvk_t, sampler, pname, param);
}

void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScissor(*rvk_t, x, y, width, height);
}

void APIENTRY glScissorArrayv(GLuint first, GLsizei count, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScissorArrayv(*rvk_t, first, count, v);
}

void APIENTRY glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScissorIndexed(*rvk_t, index, left, bottom, width, height);
}

void APIENTRY glScissorIndexedv(GLuint index, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScissorIndexedv(*rvk_t, index, v);
}

void APIENTRY glSecondaryColorP3ui(GLenum type, GLuint color) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColorP3ui(*rvk_t, type, color);
}

void APIENTRY glSecondaryColorP3uiv(GLenum type, const GLuint* color) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColorP3uiv(*rvk_t, type, color);
}

void APIENTRY glShaderBinary(GLsizei count, const GLuint* shaders, GLenum binaryFormat, const void* binary, GLsizei length) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glShaderBinary(*rvk_t, count, shaders, binaryFormat, binary, length);
}

void APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glShaderSource(*rvk_t, shader, count, string, length);
}

void APIENTRY glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glShaderStorageBlockBinding(*rvk_t, program, storageBlockIndex, storageBlockBinding);
}

void APIENTRY glSpecializeShader(GLuint shader, const GLchar* pEntryPoint, GLuint numSpecializationConstants, const GLuint* pConstantIndex, const GLuint* pConstantValue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSpecializeShader(*rvk_t, shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

void APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glStencilFunc(*rvk_t, func, ref, mask);
}

void APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glStencilFuncSeparate(*rvk_t, face, func, ref, mask);
}

void APIENTRY glStencilMask(GLuint mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glStencilMask(*rvk_t, mask);
}

void APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glStencilMaskSeparate(*rvk_t, face, mask);
}

void APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glStencilOp(*rvk_t, fail, zfail, zpass);
}

void APIENTRY glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glStencilOpSeparate(*rvk_t, face, sfail, dpfail, dppass);
}

void APIENTRY glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexBuffer(*rvk_t, target, internalformat, buffer);
}

void APIENTRY glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexBufferRange(*rvk_t, target, internalformat, buffer, offset, size);
}

void APIENTRY glTexCoordP1ui(GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP1ui(*rvk_t, type, coords);
}

void APIENTRY glTexCoordP1uiv(GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP1uiv(*rvk_t, type, coords);
}

void APIENTRY glTexCoordP2ui(GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP2ui(*rvk_t, type, coords);
}

void APIENTRY glTexCoordP2uiv(GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP2uiv(*rvk_t, type, coords);
}

void APIENTRY glTexCoordP3ui(GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP3ui(*rvk_t, type, coords);
}

void APIENTRY glTexCoordP3uiv(GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP3uiv(*rvk_t, type, coords);
}

void APIENTRY glTexCoordP4ui(GLenum type, GLuint coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP4ui(*rvk_t, type, coords);
}

void APIENTRY glTexCoordP4uiv(GLenum type, const GLuint* coords) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordP4uiv(*rvk_t, type, coords);
}

void APIENTRY glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTexImage1D(*rvk_t, target, level, internalformat, width, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, 1, 1, false), rvk_pixels_offset);
}

void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTexImage2D(*rvk_t, target, level, internalformat, width, height, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexImage2DMultisample(*rvk_t, target, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTexImage3D(*rvk_t, target, level, internalformat, width, height, depth, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_pixels_offset);
}

void APIENTRY glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexImage3DMultisample(*rvk_t, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glTexParameterIiv(GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexParameterIiv(*rvk_t, target, pname, params);
}

void APIENTRY glTexParameterIuiv(GLenum target, GLenum pname, const GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexParameterIuiv(*rvk_t, target, pname, params);
}

void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexParameterf(*rvk_t, target, pname, param);
}

void APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexParameterfv(*rvk_t, target, pname, params);
}

void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexParameteri(*rvk_t, target, pname, param);
}

void APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexParameteriv(*rvk_t, target, pname, params);
}

void APIENTRY glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexStorage1D(*rvk_t, target, levels, internalformat, width);
}

void APIENTRY glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexStorage2D(*rvk_t, target, levels, internalformat, width, height);
}

void APIENTRY glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexStorage2DMultisample(*rvk_t, target, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexStorage3D(*rvk_t, target, levels, internalformat, width, height, depth);
}

void APIENTRY glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexStorage3DMultisample(*rvk_t, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTexSubImage1D(*rvk_t, target, level, xoffset, width, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, 1, 1, false), rvk_pixels_offset);
}

void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTexSubImage2D(*rvk_t, target, level, xoffset, yoffset, width, height, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTexSubImage3D(*rvk_t, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_pixels_offset);
}

void APIENTRY glTextureBarrier(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureBarrier(*rvk_t);
}

void APIENTRY glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureBuffer(*rvk_t, texture, internalformat, buffer);
}

void APIENTRY glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureBufferRange(*rvk_t, texture, internalformat, buffer, offset, size);
}

void APIENTRY glTextureParameterIiv(GLuint texture, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterIiv(*rvk_t, texture, pname, params);
}

void APIENTRY glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterIuiv(*rvk_t, texture, pname, params);
}

void APIENTRY glTextureParameterf(GLuint texture, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterf(*rvk_t, texture, pname, param);
}

void APIENTRY glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterfv(*rvk_t, texture, pname, param);
}

void APIENTRY glTextureParameteri(GLuint texture, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameteri(*rvk_t, texture, pname, param);
}

void APIENTRY glTextureParameteriv(GLuint texture, GLenum pname, const GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameteriv(*rvk_t, texture, pname, param);
}

void APIENTRY glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage1D(*rvk_t, texture, levels, internalformat, width);
}

void APIENTRY glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage2D(*rvk_t, texture, levels, internalformat, width, height);
}

void APIENTRY glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage2DMultisample(*rvk_t, texture, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage3D(*rvk_t, texture, levels, internalformat, width, height, depth);
}

void APIENTRY glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage3DMultisample(*rvk_t, texture, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureSubImage1D(*rvk_t, texture, level, xoffset, width, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, 1, 1, false), rvk_pixels_offset);
}

void APIENTRY glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureSubImage2D(*rvk_t, texture, level, xoffset, yoffset, width, height, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureSubImage3D(*rvk_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_pixels_offset);
}

void APIENTRY glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureView(*rvk_t, texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
}

void APIENTRY glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTransformFeedbackBufferBase(*rvk_t, xfb, index, buffer);
}

void APIENTRY glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTransformFeedbackBufferRange(*rvk_t, xfb, index, buffer, offset, size);
}

void APIENTRY glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar** varyings, GLenum bufferMode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTransformFeedbackVaryings(*rvk_t, program, count, varyings, bufferMode);
}

void APIENTRY glUniform1d(GLint location, GLdouble x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1d(*rvk_t, location, x);
}

void APIENTRY glUniform1dv(GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1dv(*rvk_t, location, count, value);
}

void APIENTRY glUniform1f(GLint location, GLfloat v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1f(*rvk_t, location, v0);
}

void APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1fv(*rvk_t, location, count, value);
}

void APIENTRY glUniform1i(GLint location, GLint v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1i(*rvk_t, location, v0);
}

void APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1iv(*rvk_t, location, count, value);
}

void APIENTRY glUniform1ui(GLint location, GLuint v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1ui(*rvk_t, location, v0);
}

void APIENTRY glUniform1uiv(GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1uiv(*rvk_t, location, count, value);
}

void APIENTRY glUniform2d(GLint location, GLdouble x, GLdouble y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2d(*rvk_t, location, x, y);
}

void APIENTRY glUniform2dv(GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2dv(*rvk_t, location, count, value);
}

void APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2f(*rvk_t, location, v0, v1);
}

void APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2fv(*rvk_t, location, count, value);
}

void APIENTRY glUniform2i(GLint location, GLint v0, GLint v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2i(*rvk_t, location, v0, v1);
}

void APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2iv(*rvk_t, location, count, value);
}

void APIENTRY glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2ui(*rvk_t, location, v0, v1);
}

void APIENTRY glUniform2uiv(GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2uiv(*rvk_t, location, count, value);
}

void APIENTRY glUniform3d(GLint location, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3d(*rvk_t, location, x, y, z);
}

void APIENTRY glUniform3dv(GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3dv(*rvk_t, location, count, value);
}

void APIENTRY glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3f(*rvk_t, location, v0, v1, v2);
}

void APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3fv(*rvk_t, location, count, value);
}

void APIENTRY glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3i(*rvk_t, location, v0, v1, v2);
}

void APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3iv(*rvk_t, location, count, value);
}

void APIENTRY glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3ui(*rvk_t, location, v0, v1, v2);
}

void APIENTRY glUniform3uiv(GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3uiv(*rvk_t, location, count, value);
}

void APIENTRY glUniform4d(GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4d(*rvk_t, location, x, y, z, w);
}

void APIENTRY glUniform4dv(GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4dv(*rvk_t, location, count, value);
}

void APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4f(*rvk_t, location, v0, v1, v2, v3);
}

void APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4fv(*rvk_t, location, count, value);
}

void APIENTRY glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4i(*rvk_t, location, v0, v1, v2, v3);
}

void APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4iv(*rvk_t, location, count, value);
}

void APIENTRY glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4ui(*rvk_t, location, v0, v1, v2, v3);
}

void APIENTRY glUniform4uiv(GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4uiv(*rvk_t, location, count, value);
}

void APIENTRY glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformBlockBinding(*rvk_t, program, uniformBlockIndex, uniformBlockBinding);
}

void APIENTRY glUniformMatrix2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix2dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix2fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix2x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix2x3dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix2x3fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix2x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix2x4dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix2x4fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix3dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix3fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix3x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix3x2dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix3x2fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix3x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix3x4dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix3x4fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix4dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix4fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix4x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix4x2dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix4x2fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix4x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix4x3dv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformMatrix4x3fv(*rvk_t, location, count, transpose, value);
}

void APIENTRY glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint* indices) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformSubroutinesuiv(*rvk_t, shadertype, count, indices);
}

void APIENTRY glUseProgram(GLuint program) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUseProgram(*rvk_t, program);
}

void APIENTRY glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUseProgramStages(*rvk_t, pipeline, stages, program);
}

void APIENTRY glValidateProgram(GLuint program) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glValidateProgram(*rvk_t, program);
}

void APIENTRY glValidateProgramPipeline(GLuint pipeline) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glValidateProgramPipeline(*rvk_t, pipeline);
}

void APIENTRY glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayAttribBinding(*rvk_t, vaobj, attribindex, bindingindex);
}

void APIENTRY glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayAttribFormat(*rvk_t, vaobj, attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayAttribIFormat(*rvk_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY glVertexArrayAttribLFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayAttribLFormat(*rvk_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY glVertexArrayBindingDivisor(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayBindingDivisor(*rvk_t, vaobj, bindingindex, divisor);
}

void APIENTRY glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayElementBuffer(*rvk_t, vaobj, buffer);
}

void APIENTRY glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexBuffer(*rvk_t, vaobj, bindingindex, buffer, offset, stride);
}

void APIENTRY glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexBuffers(*rvk_t, vaobj, first, count, buffers, offsets, strides);
}

void APIENTRY glVertexAttrib1d(GLuint index, GLdouble x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib1d(*rvk_t, index, x);
}

void APIENTRY glVertexAttrib1dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib1dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib1f(GLuint index, GLfloat x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib1f(*rvk_t, index, x);
}

void APIENTRY glVertexAttrib1fv(GLuint index, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib1fv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib1s(GLuint index, GLshort x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib1s(*rvk_t, index, x);
}

void APIENTRY glVertexAttrib1sv(GLuint index, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib1sv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib2d(GLuint index, GLdouble x, GLdouble y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib2d(*rvk_t, index, x, y);
}

void APIENTRY glVertexAttrib2dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib2dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib2f(*rvk_t, index, x, y);
}

void APIENTRY glVertexAttrib2fv(GLuint index, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib2fv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib2s(GLuint index, GLshort x, GLshort y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib2s(*rvk_t, index, x, y);
}

void APIENTRY glVertexAttrib2sv(GLuint index, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib2sv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib3d(*rvk_t, index, x, y, z);
}

void APIENTRY glVertexAttrib3dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib3dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib3f(*rvk_t, index, x, y, z);
}

void APIENTRY glVertexAttrib3fv(GLuint index, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib3fv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib3s(GLuint index, GLshort x, GLshort y, GLshort z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib3s(*rvk_t, index, x, y, z);
}

void APIENTRY glVertexAttrib3sv(GLuint index, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib3sv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4Nbv(GLuint index, const GLbyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4Nbv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4Niv(GLuint index, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4Niv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4Nsv(GLuint index, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4Nsv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4Nub(*rvk_t, index, x, y, z, w);
}

void APIENTRY glVertexAttrib4Nubv(GLuint index, const GLubyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4Nubv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4Nuiv(GLuint index, const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4Nuiv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4Nusv(GLuint index, const GLushort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4Nusv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4bv(GLuint index, const GLbyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4bv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4d(*rvk_t, index, x, y, z, w);
}

void APIENTRY glVertexAttrib4dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4f(*rvk_t, index, x, y, z, w);
}

void APIENTRY glVertexAttrib4fv(GLuint index, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4fv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4iv(GLuint index, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4iv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4s(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4s(*rvk_t, index, x, y, z, w);
}

void APIENTRY glVertexAttrib4sv(GLuint index, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4sv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4ubv(GLuint index, const GLubyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4ubv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4uiv(GLuint index, const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4uiv(*rvk_t, index, v);
}

void APIENTRY glVertexAttrib4usv(GLuint index, const GLushort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttrib4usv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribBinding(GLuint attribindex, GLuint bindingindex) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribBinding(*rvk_t, attribindex, bindingindex);
}

void APIENTRY glVertexAttribDivisor(GLuint index, GLuint divisor) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribDivisor(*rvk_t, index, divisor);
}

void APIENTRY glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribFormat(*rvk_t, attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY glVertexAttribI1i(GLuint index, GLint x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI1i(*rvk_t, index, x);
}

void APIENTRY glVertexAttribI1iv(GLuint index, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI1iv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI1ui(GLuint index, GLuint x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI1ui(*rvk_t, index, x);
}

void APIENTRY glVertexAttribI1uiv(GLuint index, const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI1uiv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI2i(GLuint index, GLint x, GLint y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI2i(*rvk_t, index, x, y);
}

void APIENTRY glVertexAttribI2iv(GLuint index, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI2iv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI2ui(GLuint index, GLuint x, GLuint y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI2ui(*rvk_t, index, x, y);
}

void APIENTRY glVertexAttribI2uiv(GLuint index, const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI2uiv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI3i(*rvk_t, index, x, y, z);
}

void APIENTRY glVertexAttribI3iv(GLuint index, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI3iv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI3ui(*rvk_t, index, x, y, z);
}

void APIENTRY glVertexAttribI3uiv(GLuint index, const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI3uiv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI4bv(GLuint index, const GLbyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4bv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4i(*rvk_t, index, x, y, z, w);
}

void APIENTRY glVertexAttribI4iv(GLuint index, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4iv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI4sv(GLuint index, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4sv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI4ubv(GLuint index, const GLubyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4ubv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4ui(*rvk_t, index, x, y, z, w);
}

void APIENTRY glVertexAttribI4uiv(GLuint index, const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4uiv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribI4usv(GLuint index, const GLushort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribI4usv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribIFormat(*rvk_t, attribindex, size, type, relativeoffset);
}

void APIENTRY glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribIPointer(*rvk_t, index, size, type, stride, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)));
}

void APIENTRY glVertexAttribL1d(GLuint index, GLdouble x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL1d(*rvk_t, index, x);
}

void APIENTRY glVertexAttribL1dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL1dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribL2d(GLuint index, GLdouble x, GLdouble y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL2d(*rvk_t, index, x, y);
}

void APIENTRY glVertexAttribL2dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL2dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL3d(*rvk_t, index, x, y, z);
}

void APIENTRY glVertexAttribL3dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL3dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL4d(*rvk_t, index, x, y, z, w);
}

void APIENTRY glVertexAttribL4dv(GLuint index, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL4dv(*rvk_t, index, v);
}

void APIENTRY glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribLFormat(*rvk_t, attribindex, size, type, relativeoffset);
}

void APIENTRY glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribLPointer(*rvk_t, index, size, type, stride, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)));
}

void APIENTRY glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP1ui(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP1uiv(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP2ui(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP2uiv(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP3ui(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP3uiv(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP4ui(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribP4uiv(*rvk_t, index, type, normalized, value);
}

void APIENTRY glVertexBindingDivisor(GLuint bindingindex, GLuint divisor) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexBindingDivisor(*rvk_t, bindingindex, divisor);
}

void APIENTRY glVertexP2ui(GLenum type, GLuint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexP2ui(*rvk_t, type, value);
}

void APIENTRY glVertexP2uiv(GLenum type, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexP2uiv(*rvk_t, type, value);
}

void APIENTRY glVertexP3ui(GLenum type, GLuint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexP3ui(*rvk_t, type, value);
}

void APIENTRY glVertexP3uiv(GLenum type, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexP3uiv(*rvk_t, type, value);
}

void APIENTRY glVertexP4ui(GLenum type, GLuint value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexP4ui(*rvk_t, type, value);
}

void APIENTRY glVertexP4uiv(GLenum type, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexP4uiv(*rvk_t, type, value);
}

void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glViewport(*rvk_t, x, y, width, height);
}

void APIENTRY glViewportArrayv(GLuint first, GLsizei count, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glViewportArrayv(*rvk_t, first, count, v);
}

void APIENTRY glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glViewportIndexedf(*rvk_t, index, x, y, w, h);
}

void APIENTRY glViewportIndexedfv(GLuint index, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glViewportIndexedfv(*rvk_t, index, v);
}

void APIENTRY glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWaitSync(*rvk_t, reinterpret_cast<std::uint64_t>(sync), flags, timeout);
}

void APIENTRY glAccum(GLenum op, GLfloat value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glAccum(*rvk_t, op, value);
}

void APIENTRY glAlphaFunc(GLenum func, GLfloat ref) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glAlphaFunc(*rvk_t, func, ref);
}

GLboolean APIENTRY glAreTexturesResident(GLsizei n, const GLuint* textures, GLboolean* residences) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glAreTexturesResident(*rvk_t, n, textures, residences);
}

void APIENTRY glBegin(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBegin(*rvk_t, mode);
}

void APIENTRY glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte* bitmap) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBitmap(*rvk_t, width, height, xorig, yorig, xmove, ymove, bitmap);
}

void APIENTRY glCallList(GLuint list) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCallList(*rvk_t, list);
}

void APIENTRY glCallLists(GLsizei n, GLenum type, const void* lists) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCallLists(*rvk_t, n, type, lists);
}

void APIENTRY glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearAccum(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glClearIndex(GLfloat c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClearIndex(*rvk_t, c);
}

void APIENTRY glClipPlane(GLenum plane, const GLdouble* equation) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClipPlane(*rvk_t, plane, equation);
}

void APIENTRY glColor3b(GLbyte red, GLbyte green, GLbyte blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3b(*rvk_t, red, green, blue);
}

void APIENTRY glColor3bv(const GLbyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3bv(*rvk_t, v);
}

void APIENTRY glColor3d(GLdouble red, GLdouble green, GLdouble blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3d(*rvk_t, red, green, blue);
}

void APIENTRY glColor3dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3dv(*rvk_t, v);
}

void APIENTRY glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3f(*rvk_t, red, green, blue);
}

void APIENTRY glColor3fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3fv(*rvk_t, v);
}

void APIENTRY glColor3i(GLint red, GLint green, GLint blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3i(*rvk_t, red, green, blue);
}

void APIENTRY glColor3iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3iv(*rvk_t, v);
}

void APIENTRY glColor3s(GLshort red, GLshort green, GLshort blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3s(*rvk_t, red, green, blue);
}

void APIENTRY glColor3sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3sv(*rvk_t, v);
}

void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3ub(*rvk_t, red, green, blue);
}

void APIENTRY glColor3ubv(const GLubyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3ubv(*rvk_t, v);
}

void APIENTRY glColor3ui(GLuint red, GLuint green, GLuint blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3ui(*rvk_t, red, green, blue);
}

void APIENTRY glColor3uiv(const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3uiv(*rvk_t, v);
}

void APIENTRY glColor3us(GLushort red, GLushort green, GLushort blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3us(*rvk_t, red, green, blue);
}

void APIENTRY glColor3usv(const GLushort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor3usv(*rvk_t, v);
}

void APIENTRY glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4b(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4bv(const GLbyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4bv(*rvk_t, v);
}

void APIENTRY glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4d(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4dv(*rvk_t, v);
}

void APIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4f(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4fv(*rvk_t, v);
}

void APIENTRY glColor4i(GLint red, GLint green, GLint blue, GLint alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4i(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4iv(*rvk_t, v);
}

void APIENTRY glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4s(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4sv(*rvk_t, v);
}

void APIENTRY glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4ub(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4ubv(const GLubyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4ubv(*rvk_t, v);
}

void APIENTRY glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4ui(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4uiv(const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4uiv(*rvk_t, v);
}

void APIENTRY glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4us(*rvk_t, red, green, blue, alpha);
}

void APIENTRY glColor4usv(const GLushort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColor4usv(*rvk_t, v);
}

void APIENTRY glColorMaterial(GLenum face, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorMaterial(*rvk_t, face, mode);
}

void APIENTRY glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyPixels(*rvk_t, x, y, width, height, type);
}

void APIENTRY glDeleteLists(GLuint list, GLsizei range) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteLists(*rvk_t, list, range);
}

void APIENTRY glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::flush_persistent_mappings();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glDrawPixels(*rvk_t, width, height, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glEdgeFlag(GLboolean flag) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEdgeFlag(*rvk_t, flag);
}

void APIENTRY glEdgeFlagv(const GLboolean* flag) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEdgeFlagv(*rvk_t, flag);
}

void APIENTRY glEnd(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnd(*rvk_t);
}

void APIENTRY glEndList(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEndList(*rvk_t);
}

void APIENTRY glEvalCoord1d(GLdouble u) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord1d(*rvk_t, u);
}

void APIENTRY glEvalCoord1dv(const GLdouble* u) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord1dv(*rvk_t, u);
}

void APIENTRY glEvalCoord1f(GLfloat u) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord1f(*rvk_t, u);
}

void APIENTRY glEvalCoord1fv(const GLfloat* u) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord1fv(*rvk_t, u);
}

void APIENTRY glEvalCoord2d(GLdouble u, GLdouble v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord2d(*rvk_t, u, v);
}

void APIENTRY glEvalCoord2dv(const GLdouble* u) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord2dv(*rvk_t, u);
}

void APIENTRY glEvalCoord2f(GLfloat u, GLfloat v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord2f(*rvk_t, u, v);
}

void APIENTRY glEvalCoord2fv(const GLfloat* u) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalCoord2fv(*rvk_t, u);
}

void APIENTRY glEvalMesh1(GLenum mode, GLint i1, GLint i2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalMesh1(*rvk_t, mode, i1, i2);
}

void APIENTRY glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalMesh2(*rvk_t, mode, i1, i2, j1, j2);
}

void APIENTRY glEvalPoint1(GLint i) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalPoint1(*rvk_t, i);
}

void APIENTRY glEvalPoint2(GLint i, GLint j) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEvalPoint2(*rvk_t, i, j);
}

void APIENTRY glFeedbackBuffer(GLsizei size, GLenum type, GLfloat* buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFeedbackBuffer(*rvk_t, size, type, buffer);
}

void APIENTRY glFogCoordd(GLdouble coord) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogCoordd(*rvk_t, coord);
}

void APIENTRY glFogCoorddv(const GLdouble* coord) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogCoorddv(*rvk_t, coord);
}

void APIENTRY glFogCoordf(GLfloat coord) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogCoordf(*rvk_t, coord);
}

void APIENTRY glFogCoordfv(const GLfloat* coord) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogCoordfv(*rvk_t, coord);
}

void APIENTRY glFogf(GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogf(*rvk_t, pname, param);
}

void APIENTRY glFogfv(GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogfv(*rvk_t, pname, params);
}

void APIENTRY glFogi(GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogi(*rvk_t, pname, param);
}

void APIENTRY glFogiv(GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogiv(*rvk_t, pname, params);
}

void APIENTRY glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFrustum(*rvk_t, left, right, bottom, top, zNear, zFar);
}

GLuint APIENTRY glGenLists(GLsizei range) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGenLists(*rvk_t, range);
}

void APIENTRY glGetClipPlane(GLenum plane, GLdouble* equation) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetClipPlane(*rvk_t, plane, equation);
}

void APIENTRY glGetLightfv(GLenum light, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetLightfv(*rvk_t, light, pname, params);
}

void APIENTRY glGetLightiv(GLenum light, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetLightiv(*rvk_t, light, pname, params);
}

void APIENTRY glGetMapdv(GLenum target, GLenum query, GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMapdv(*rvk_t, target, query, v);
}

void APIENTRY glGetMapfv(GLenum target, GLenum query, GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMapfv(*rvk_t, target, query, v);
}

void APIENTRY glGetMapiv(GLenum target, GLenum query, GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMapiv(*rvk_t, target, query, v);
}

void APIENTRY glGetMaterialfv(GLenum face, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMaterialfv(*rvk_t, face, pname, params);
}

void APIENTRY glGetMaterialiv(GLenum face, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMaterialiv(*rvk_t, face, pname, params);
}

void APIENTRY glGetPixelMapfv(GLenum map, GLfloat* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetPixelMapfv(*rvk_t, map, values);
}

void APIENTRY glGetPixelMapuiv(GLenum map, GLuint* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetPixelMapuiv(*rvk_t, map, values);
}

void APIENTRY glGetPixelMapusv(GLenum map, GLushort* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetPixelMapusv(*rvk_t, map, values);
}

void APIENTRY glGetPolygonStipple(GLubyte* mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetPolygonStipple(*rvk_t, mask);
}

void APIENTRY glGetTexEnvfv(GLenum target, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexEnvfv(*rvk_t, target, pname, params);
}

void APIENTRY glGetTexEnviv(GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexEnviv(*rvk_t, target, pname, params);
}

void APIENTRY glGetTexGendv(GLenum coord, GLenum pname, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexGendv(*rvk_t, coord, pname, params);
}

void APIENTRY glGetTexGenfv(GLenum coord, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexGenfv(*rvk_t, coord, pname, params);
}

void APIENTRY glGetTexGeniv(GLenum coord, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTexGeniv(*rvk_t, coord, pname, params);
}

void APIENTRY glIndexMask(GLuint mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexMask(*rvk_t, mask);
}

void APIENTRY glIndexd(GLdouble c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexd(*rvk_t, c);
}

void APIENTRY glIndexdv(GLdouble* c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexdv(*rvk_t, c);
}

void APIENTRY glIndexf(GLfloat c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexf(*rvk_t, c);
}

void APIENTRY glIndexfv(GLfloat* c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexfv(*rvk_t, c);
}

void APIENTRY glIndexi(GLint c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexi(*rvk_t, c);
}

void APIENTRY glIndexiv(GLint* c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexiv(*rvk_t, c);
}

void APIENTRY glIndexs(GLshort c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexs(*rvk_t, c);
}

void APIENTRY glIndexsv(GLshort* c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexsv(*rvk_t, c);
}

void APIENTRY glIndexub(GLubyte c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexub(*rvk_t, c);
}

void APIENTRY glIndexubv(GLubyte* c) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexubv(*rvk_t, c);
}

void APIENTRY glInitNames(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glInitNames(*rvk_t);
}

GLboolean APIENTRY glIsList(GLuint list) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsList(*rvk_t, list);
}

void APIENTRY glLightModelf(GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLightModelf(*rvk_t, pname, param);
}

void APIENTRY glLightModelfv(GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLightModelfv(*rvk_t, pname, params);
}

void APIENTRY glLightModeli(GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLightModeli(*rvk_t, pname, param);
}

void APIENTRY glLightModeliv(GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLightModeliv(*rvk_t, pname, params);
}

void APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLightf(*rvk_t, light, pname, param);
}

void APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLightfv(*rvk_t, light, pname, params);
}

void APIENTRY glLighti(GLenum light, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLighti(*rvk_t, light, pname, param);
}

void APIENTRY glLightiv(GLenum light, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLightiv(*rvk_t, light, pname, params);
}

void APIENTRY glLineStipple(GLint factor, GLushort pattern) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLineStipple(*rvk_t, factor, pattern);
}

void APIENTRY glListBase(GLuint base) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glListBase(*rvk_t, base);
}

void APIENTRY glLoadIdentity(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLoadIdentity(*rvk_t);
}

void APIENTRY glLoadMatrixd(const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLoadMatrixd(*rvk_t, m);
}

void APIENTRY glLoadMatrixf(const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLoadMatrixf(*rvk_t, m);
}

void APIENTRY glLoadName(GLuint name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLoadName(*rvk_t, name);
}

void APIENTRY glLoadTransposeMatrixd(const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLoadTransposeMatrixd(*rvk_t, m);
}

void APIENTRY glLoadTransposeMatrixf(const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glLoadTransposeMatrixf(*rvk_t, m);
}

void APIENTRY glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble* points) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMap1d(*rvk_t, target, u1, u2, stride, order, points);
}

void APIENTRY glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat* points) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMap1f(*rvk_t, target, u1, u2, stride, order, points);
}

void APIENTRY glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble* points) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMap2d(*rvk_t, target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

void APIENTRY glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat* points) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMap2f(*rvk_t, target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

void APIENTRY glMapGrid1d(GLint un, GLdouble u1, GLdouble u2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMapGrid1d(*rvk_t, un, u1, u2);
}

void APIENTRY glMapGrid1f(GLint un, GLfloat u1, GLfloat u2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMapGrid1f(*rvk_t, un, u1, u2);
}

void APIENTRY glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMapGrid2d(*rvk_t, un, u1, u2, vn, v1, v2);
}

void APIENTRY glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMapGrid2f(*rvk_t, un, u1, u2, vn, v1, v2);
}

void APIENTRY glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMaterialf(*rvk_t, face, pname, param);
}

void APIENTRY glMaterialfv(GLenum face, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMaterialfv(*rvk_t, face, pname, params);
}

void APIENTRY glMateriali(GLenum face, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMateriali(*rvk_t, face, pname, param);
}

void APIENTRY glMaterialiv(GLenum face, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMaterialiv(*rvk_t, face, pname, params);
}

void APIENTRY glMatrixMode(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixMode(*rvk_t, mode);
}

void APIENTRY glMultMatrixd(const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultMatrixd(*rvk_t, m);
}

void APIENTRY glMultMatrixf(const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultMatrixf(*rvk_t, m);
}

void APIENTRY glMultTransposeMatrixd(const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultTransposeMatrixd(*rvk_t, m);
}

void APIENTRY glMultTransposeMatrixf(const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultTransposeMatrixf(*rvk_t, m);
}

void APIENTRY glMultiTexCoord1d(GLenum target, GLdouble s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1d(*rvk_t, target, s);
}

void APIENTRY glMultiTexCoord1dv(GLenum target, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1dv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord1f(GLenum target, GLfloat s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1f(*rvk_t, target, s);
}

void APIENTRY glMultiTexCoord1fv(GLenum target, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1fv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord1i(GLenum target, GLint s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1i(*rvk_t, target, s);
}

void APIENTRY glMultiTexCoord1iv(GLenum target, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1iv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord1s(GLenum target, GLshort s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1s(*rvk_t, target, s);
}

void APIENTRY glMultiTexCoord1sv(GLenum target, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord1sv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord2d(GLenum target, GLdouble s, GLdouble t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2d(*rvk_t, target, s, t);
}

void APIENTRY glMultiTexCoord2dv(GLenum target, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2dv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2f(*rvk_t, target, s, t);
}

void APIENTRY glMultiTexCoord2fv(GLenum target, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2fv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord2i(GLenum target, GLint s, GLint t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2i(*rvk_t, target, s, t);
}

void APIENTRY glMultiTexCoord2iv(GLenum target, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2iv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord2s(GLenum target, GLshort s, GLshort t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2s(*rvk_t, target, s, t);
}

void APIENTRY glMultiTexCoord2sv(GLenum target, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord2sv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord3d(GLenum target, GLdouble s, GLdouble t, GLdouble r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3d(*rvk_t, target, s, t, r);
}

void APIENTRY glMultiTexCoord3dv(GLenum target, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3dv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord3f(GLenum target, GLfloat s, GLfloat t, GLfloat r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3f(*rvk_t, target, s, t, r);
}

void APIENTRY glMultiTexCoord3fv(GLenum target, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3fv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord3i(GLenum target, GLint s, GLint t, GLint r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3i(*rvk_t, target, s, t, r);
}

void APIENTRY glMultiTexCoord3iv(GLenum target, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3iv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord3s(GLenum target, GLshort s, GLshort t, GLshort r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3s(*rvk_t, target, s, t, r);
}

void APIENTRY glMultiTexCoord3sv(GLenum target, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord3sv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord4d(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4d(*rvk_t, target, s, t, r, q);
}

void APIENTRY glMultiTexCoord4dv(GLenum target, const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4dv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4f(*rvk_t, target, s, t, r, q);
}

void APIENTRY glMultiTexCoord4fv(GLenum target, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4fv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord4i(GLenum target, GLint s, GLint t, GLint r, GLint q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4i(*rvk_t, target, s, t, r, q);
}

void APIENTRY glMultiTexCoord4iv(GLenum target, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4iv(*rvk_t, target, v);
}

void APIENTRY glMultiTexCoord4s(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4s(*rvk_t, target, s, t, r, q);
}

void APIENTRY glMultiTexCoord4sv(GLenum target, const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoord4sv(*rvk_t, target, v);
}

void APIENTRY glNewList(GLuint list, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNewList(*rvk_t, list, mode);
}

void APIENTRY glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3b(*rvk_t, nx, ny, nz);
}

void APIENTRY glNormal3bv(const GLbyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3bv(*rvk_t, v);
}

void APIENTRY glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3d(*rvk_t, nx, ny, nz);
}

void APIENTRY glNormal3dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3dv(*rvk_t, v);
}

void APIENTRY glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3f(*rvk_t, nx, ny, nz);
}

void APIENTRY glNormal3fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3fv(*rvk_t, v);
}

void APIENTRY glNormal3i(GLint nx, GLint ny, GLint nz) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3i(*rvk_t, nx, ny, nz);
}

void APIENTRY glNormal3iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3iv(*rvk_t, v);
}

void APIENTRY glNormal3s(GLshort nx, GLshort ny, GLshort nz) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3s(*rvk_t, nx, ny, nz);
}

void APIENTRY glNormal3sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormal3sv(*rvk_t, v);
}

void APIENTRY glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glOrtho(*rvk_t, left, right, bottom, top, zNear, zFar);
}

void APIENTRY glPassThrough(GLfloat token) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPassThrough(*rvk_t, token);
}

void APIENTRY glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPixelMapfv(*rvk_t, map, mapsize, values);
}

void APIENTRY glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPixelMapuiv(*rvk_t, map, mapsize, values);
}

void APIENTRY glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPixelMapusv(*rvk_t, map, mapsize, values);
}

void APIENTRY glPixelTransferf(GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPixelTransferf(*rvk_t, pname, param);
}

void APIENTRY glPixelTransferi(GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPixelTransferi(*rvk_t, pname, param);
}

void APIENTRY glPixelZoom(GLfloat xfactor, GLfloat yfactor) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPixelZoom(*rvk_t, xfactor, yfactor);
}

void APIENTRY glPolygonStipple(const GLubyte* mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPolygonStipple(*rvk_t, mask);
}

void APIENTRY glPopAttrib(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPopAttrib(*rvk_t);
}

void APIENTRY glPopClientAttrib(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPopClientAttrib(*rvk_t);
}

void APIENTRY glPopMatrix(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPopMatrix(*rvk_t);
}

void APIENTRY glPopName(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPopName(*rvk_t);
}

void APIENTRY glPrioritizeTextures(GLsizei n, const GLuint* textures, const GLfloat* priorities) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPrioritizeTextures(*rvk_t, n, textures, priorities);
}

void APIENTRY glPushAttrib(GLbitfield mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPushAttrib(*rvk_t, mask);
}

void APIENTRY glPushClientAttrib(GLbitfield mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPushClientAttrib(*rvk_t, mask);
}

void APIENTRY glPushMatrix(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPushMatrix(*rvk_t);
}

void APIENTRY glPushName(GLuint name) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPushName(*rvk_t, name);
}

void APIENTRY glRasterPos2d(GLdouble x, GLdouble y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2d(*rvk_t, x, y);
}

void APIENTRY glRasterPos2dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2dv(*rvk_t, v);
}

void APIENTRY glRasterPos2f(GLfloat x, GLfloat y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2f(*rvk_t, x, y);
}

void APIENTRY glRasterPos2fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2fv(*rvk_t, v);
}

void APIENTRY glRasterPos2i(GLint x, GLint y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2i(*rvk_t, x, y);
}

void APIENTRY glRasterPos2iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2iv(*rvk_t, v);
}

void APIENTRY glRasterPos2s(GLshort x, GLshort y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2s(*rvk_t, x, y);
}

void APIENTRY glRasterPos2sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos2sv(*rvk_t, v);
}

void APIENTRY glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3d(*rvk_t, x, y, z);
}

void APIENTRY glRasterPos3dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3dv(*rvk_t, v);
}

void APIENTRY glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3f(*rvk_t, x, y, z);
}

void APIENTRY glRasterPos3fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3fv(*rvk_t, v);
}

void APIENTRY glRasterPos3i(GLint x, GLint y, GLint z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3i(*rvk_t, x, y, z);
}

void APIENTRY glRasterPos3iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3iv(*rvk_t, v);
}

void APIENTRY glRasterPos3s(GLshort x, GLshort y, GLshort z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3s(*rvk_t, x, y, z);
}

void APIENTRY glRasterPos3sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos3sv(*rvk_t, v);
}

void APIENTRY glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4d(*rvk_t, x, y, z, w);
}

void APIENTRY glRasterPos4dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4dv(*rvk_t, v);
}

void APIENTRY glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4f(*rvk_t, x, y, z, w);
}

void APIENTRY glRasterPos4fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4fv(*rvk_t, v);
}

void APIENTRY glRasterPos4i(GLint x, GLint y, GLint z, GLint w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4i(*rvk_t, x, y, z, w);
}

void APIENTRY glRasterPos4iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4iv(*rvk_t, v);
}

void APIENTRY glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4s(*rvk_t, x, y, z, w);
}

void APIENTRY glRasterPos4sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRasterPos4sv(*rvk_t, v);
}

void APIENTRY glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRectd(*rvk_t, x1, y1, x2, y2);
}

void APIENTRY glRectdv(const GLdouble* v1, const GLdouble* v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRectdv(*rvk_t, v1, v2);
}

void APIENTRY glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRectf(*rvk_t, x1, y1, x2, y2);
}

void APIENTRY glRectfv(const GLfloat* v1, const GLfloat* v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRectfv(*rvk_t, v1, v2);
}

void APIENTRY glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRecti(*rvk_t, x1, y1, x2, y2);
}

void APIENTRY glRectiv(const GLint* v1, const GLint* v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRectiv(*rvk_t, v1, v2);
}

void APIENTRY glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRects(*rvk_t, x1, y1, x2, y2);
}

void APIENTRY glRectsv(const GLshort* v1, const GLshort* v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRectsv(*rvk_t, v1, v2);
}

GLint APIENTRY glRenderMode(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glRenderMode(*rvk_t, mode);
}

void APIENTRY glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRotated(*rvk_t, angle, x, y, z);
}

void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glRotatef(*rvk_t, angle, x, y, z);
}

void APIENTRY glScaled(GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScaled(*rvk_t, x, y, z);
}

void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScalef(*rvk_t, x, y, z);
}

void APIENTRY glSecondaryColor3b(GLbyte red, GLbyte green, GLbyte blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3b(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3bv(const GLbyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3bv(*rvk_t, v);
}

void APIENTRY glSecondaryColor3d(GLdouble red, GLdouble green, GLdouble blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3d(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3dv(*rvk_t, v);
}

void APIENTRY glSecondaryColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3f(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3fv(*rvk_t, v);
}

void APIENTRY glSecondaryColor3i(GLint red, GLint green, GLint blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3i(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3iv(*rvk_t, v);
}

void APIENTRY glSecondaryColor3s(GLshort red, GLshort green, GLshort blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3s(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3sv(*rvk_t, v);
}

void APIENTRY glSecondaryColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3ub(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3ubv(const GLubyte* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3ubv(*rvk_t, v);
}

void APIENTRY glSecondaryColor3ui(GLuint red, GLuint green, GLuint blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3ui(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3uiv(const GLuint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3uiv(*rvk_t, v);
}

void APIENTRY glSecondaryColor3us(GLushort red, GLushort green, GLushort blue) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3us(*rvk_t, red, green, blue);
}

void APIENTRY glSecondaryColor3usv(const GLushort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColor3usv(*rvk_t, v);
}

void APIENTRY glSelectBuffer(GLsizei size, GLuint* buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSelectBuffer(*rvk_t, size, buffer);
}

void APIENTRY glShadeModel(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glShadeModel(*rvk_t, mode);
}

void APIENTRY glTexCoord1d(GLdouble s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1d(*rvk_t, s);
}

void APIENTRY glTexCoord1dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1dv(*rvk_t, v);
}

void APIENTRY glTexCoord1f(GLfloat s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1f(*rvk_t, s);
}

void APIENTRY glTexCoord1fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1fv(*rvk_t, v);
}

void APIENTRY glTexCoord1i(GLint s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1i(*rvk_t, s);
}

void APIENTRY glTexCoord1iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1iv(*rvk_t, v);
}

void APIENTRY glTexCoord1s(GLshort s) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1s(*rvk_t, s);
}

void APIENTRY glTexCoord1sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord1sv(*rvk_t, v);
}

void APIENTRY glTexCoord2d(GLdouble s, GLdouble t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2d(*rvk_t, s, t);
}

void APIENTRY glTexCoord2dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2dv(*rvk_t, v);
}

void APIENTRY glTexCoord2f(GLfloat s, GLfloat t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2f(*rvk_t, s, t);
}

void APIENTRY glTexCoord2fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2fv(*rvk_t, v);
}

void APIENTRY glTexCoord2i(GLint s, GLint t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2i(*rvk_t, s, t);
}

void APIENTRY glTexCoord2iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2iv(*rvk_t, v);
}

void APIENTRY glTexCoord2s(GLshort s, GLshort t) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2s(*rvk_t, s, t);
}

void APIENTRY glTexCoord2sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord2sv(*rvk_t, v);
}

void APIENTRY glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3d(*rvk_t, s, t, r);
}

void APIENTRY glTexCoord3dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3dv(*rvk_t, v);
}

void APIENTRY glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3f(*rvk_t, s, t, r);
}

void APIENTRY glTexCoord3fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3fv(*rvk_t, v);
}

void APIENTRY glTexCoord3i(GLint s, GLint t, GLint r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3i(*rvk_t, s, t, r);
}

void APIENTRY glTexCoord3iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3iv(*rvk_t, v);
}

void APIENTRY glTexCoord3s(GLshort s, GLshort t, GLshort r) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3s(*rvk_t, s, t, r);
}

void APIENTRY glTexCoord3sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord3sv(*rvk_t, v);
}

void APIENTRY glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4d(*rvk_t, s, t, r, q);
}

void APIENTRY glTexCoord4dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4dv(*rvk_t, v);
}

void APIENTRY glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4f(*rvk_t, s, t, r, q);
}

void APIENTRY glTexCoord4fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4fv(*rvk_t, v);
}

void APIENTRY glTexCoord4i(GLint s, GLint t, GLint r, GLint q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4i(*rvk_t, s, t, r, q);
}

void APIENTRY glTexCoord4iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4iv(*rvk_t, v);
}

void APIENTRY glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4s(*rvk_t, s, t, r, q);
}

void APIENTRY glTexCoord4sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoord4sv(*rvk_t, v);
}

void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexEnvf(*rvk_t, target, pname, param);
}

void APIENTRY glTexEnvfv(GLenum target, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexEnvfv(*rvk_t, target, pname, params);
}

void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexEnvi(*rvk_t, target, pname, param);
}

void APIENTRY glTexEnviv(GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexEnviv(*rvk_t, target, pname, params);
}

void APIENTRY glTexGend(GLenum coord, GLenum pname, GLdouble param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexGend(*rvk_t, coord, pname, param);
}

void APIENTRY glTexGendv(GLenum coord, GLenum pname, const GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexGendv(*rvk_t, coord, pname, params);
}

void APIENTRY glTexGenf(GLenum coord, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexGenf(*rvk_t, coord, pname, param);
}

void APIENTRY glTexGenfv(GLenum coord, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexGenfv(*rvk_t, coord, pname, params);
}

void APIENTRY glTexGeni(GLenum coord, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexGeni(*rvk_t, coord, pname, param);
}

void APIENTRY glTexGeniv(GLenum coord, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexGeniv(*rvk_t, coord, pname, params);
}

void APIENTRY glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTranslated(*rvk_t, x, y, z);
}

void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTranslatef(*rvk_t, x, y, z);
}

void APIENTRY glVertex2d(GLdouble x, GLdouble y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2d(*rvk_t, x, y);
}

void APIENTRY glVertex2dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2dv(*rvk_t, v);
}

void APIENTRY glVertex2f(GLfloat x, GLfloat y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2f(*rvk_t, x, y);
}

void APIENTRY glVertex2fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2fv(*rvk_t, v);
}

void APIENTRY glVertex2i(GLint x, GLint y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2i(*rvk_t, x, y);
}

void APIENTRY glVertex2iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2iv(*rvk_t, v);
}

void APIENTRY glVertex2s(GLshort x, GLshort y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2s(*rvk_t, x, y);
}

void APIENTRY glVertex2sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex2sv(*rvk_t, v);
}

void APIENTRY glVertex3d(GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3d(*rvk_t, x, y, z);
}

void APIENTRY glVertex3dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3dv(*rvk_t, v);
}

void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3f(*rvk_t, x, y, z);
}

void APIENTRY glVertex3fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3fv(*rvk_t, v);
}

void APIENTRY glVertex3i(GLint x, GLint y, GLint z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3i(*rvk_t, x, y, z);
}

void APIENTRY glVertex3iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3iv(*rvk_t, v);
}

void APIENTRY glVertex3s(GLshort x, GLshort y, GLshort z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3s(*rvk_t, x, y, z);
}

void APIENTRY glVertex3sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex3sv(*rvk_t, v);
}

void APIENTRY glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4d(*rvk_t, x, y, z, w);
}

void APIENTRY glVertex4dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4dv(*rvk_t, v);
}

void APIENTRY glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4f(*rvk_t, x, y, z, w);
}

void APIENTRY glVertex4fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4fv(*rvk_t, v);
}

void APIENTRY glVertex4i(GLint x, GLint y, GLint z, GLint w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4i(*rvk_t, x, y, z, w);
}

void APIENTRY glVertex4iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4iv(*rvk_t, v);
}

void APIENTRY glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4s(*rvk_t, x, y, z, w);
}

void APIENTRY glVertex4sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertex4sv(*rvk_t, v);
}

void APIENTRY glWindowPos2d(GLdouble x, GLdouble y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2d(*rvk_t, x, y);
}

void APIENTRY glWindowPos2dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2dv(*rvk_t, v);
}

void APIENTRY glWindowPos2f(GLfloat x, GLfloat y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2f(*rvk_t, x, y);
}

void APIENTRY glWindowPos2fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2fv(*rvk_t, v);
}

void APIENTRY glWindowPos2i(GLint x, GLint y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2i(*rvk_t, x, y);
}

void APIENTRY glWindowPos2iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2iv(*rvk_t, v);
}

void APIENTRY glWindowPos2s(GLshort x, GLshort y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2s(*rvk_t, x, y);
}

void APIENTRY glWindowPos2sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos2sv(*rvk_t, v);
}

void APIENTRY glWindowPos3d(GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3d(*rvk_t, x, y, z);
}

void APIENTRY glWindowPos3dv(const GLdouble* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3dv(*rvk_t, v);
}

void APIENTRY glWindowPos3f(GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3f(*rvk_t, x, y, z);
}

void APIENTRY glWindowPos3fv(const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3fv(*rvk_t, v);
}

void APIENTRY glWindowPos3i(GLint x, GLint y, GLint z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3i(*rvk_t, x, y, z);
}

void APIENTRY glWindowPos3iv(const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3iv(*rvk_t, v);
}

void APIENTRY glWindowPos3s(GLshort x, GLshort y, GLshort z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3s(*rvk_t, x, y, z);
}

void APIENTRY glWindowPos3sv(const GLshort* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glWindowPos3sv(*rvk_t, v);
}

void APIENTRY glDeleteObjectARB(GLhandleARB obj) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDeleteObjectARB(*rvk_t, obj);
}

GLhandleARB APIENTRY glGetHandleARB(GLenum pname) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetHandleARB(*rvk_t, pname);
}

void APIENTRY glGetObjectParameterfvARB(GLhandleARB obj, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetObjectParameterfvARB(*rvk_t, obj, pname, params);
}

void APIENTRY glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetObjectParameterivARB(*rvk_t, obj, pname, params);
}

void APIENTRY glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei* length, GLcharARB* infoLog) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetInfoLogARB(*rvk_t, obj, maxLength, length, infoLog);
}

void APIENTRY glGetAttachedObjectsARB(GLhandleARB containerObj, GLsizei maxCount, GLsizei* count, GLhandleARB* obj) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetAttachedObjectsARB(*rvk_t, containerObj, maxCount, count, obj);
}

void APIENTRY glTexPageCommitmentARB(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexPageCommitmentARB(*rvk_t, target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void APIENTRY glBufferPageCommitmentARB(GLenum target, GLintptr offset, GLsizeiptr size, GLboolean commit) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBufferPageCommitmentARB(*rvk_t, target, offset, size, commit);
}

void APIENTRY glNamedBufferPageCommitmentEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedBufferPageCommitmentEXT(*rvk_t, buffer, offset, size, commit);
}

void APIENTRY glNamedBufferPageCommitmentARB(GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedBufferPageCommitmentARB(*rvk_t, buffer, offset, size, commit);
}

void APIENTRY glTexturePageCommitmentEXT(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexturePageCommitmentEXT(*rvk_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void APIENTRY glMaxShaderCompilerThreadsARB(GLuint count) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMaxShaderCompilerThreadsARB(*rvk_t, count);
}

void APIENTRY glMaxShaderCompilerThreadsKHR(GLuint count) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMaxShaderCompilerThreadsKHR(*rvk_t, count);
}

void APIENTRY glScissorExclusiveNV(GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScissorExclusiveNV(*rvk_t, x, y, width, height);
}

void APIENTRY glScissorExclusiveArrayvNV(GLuint first, GLsizei count, const GLint* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glScissorExclusiveArrayvNV(*rvk_t, first, count, v);
}

void APIENTRY glFramebufferSampleLocationsfvNV(GLenum target, GLuint start, GLsizei count, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferSampleLocationsfvNV(*rvk_t, target, start, count, v);
}

void APIENTRY glNamedFramebufferSampleLocationsfvNV(GLuint framebuffer, GLuint start, GLsizei count, const GLfloat* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferSampleLocationsfvNV(*rvk_t, framebuffer, start, count, v);
}

void APIENTRY glResolveDepthValuesNV(void) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glResolveDepthValuesNV(*rvk_t);
}

GLuint64 APIENTRY glGetTextureHandleARB(GLuint texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetTextureHandleARB(*rvk_t, texture);
}

GLuint64 APIENTRY glGetTextureSamplerHandleARB(GLuint texture, GLuint sampler) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetTextureSamplerHandleARB(*rvk_t, texture, sampler);
}

void APIENTRY glMakeTextureHandleResidentARB(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeTextureHandleResidentARB(*rvk_t, handle);
}

void APIENTRY glMakeTextureHandleNonResidentARB(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeTextureHandleNonResidentARB(*rvk_t, handle);
}

GLuint64 APIENTRY glGetImageHandleARB(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetImageHandleARB(*rvk_t, texture, level, layered, layer, format);
}

void APIENTRY glMakeImageHandleResidentARB(GLuint64 handle, GLenum access) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeImageHandleResidentARB(*rvk_t, handle, access);
}

void APIENTRY glMakeImageHandleNonResidentARB(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeImageHandleNonResidentARB(*rvk_t, handle);
}

void APIENTRY glUniformHandleui64ARB(GLint location, GLuint64 value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformHandleui64ARB(*rvk_t, location, value);
}

void APIENTRY glUniformHandleui64vARB(GLint location, GLsizei count, const GLuint64* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformHandleui64vARB(*rvk_t, location, count, value);
}

void APIENTRY glProgramUniformHandleui64ARB(GLuint program, GLint location, GLuint64 value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformHandleui64ARB(*rvk_t, program, location, value);
}

void APIENTRY glProgramUniformHandleui64vARB(GLuint program, GLint location, GLsizei count, const GLuint64* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformHandleui64vARB(*rvk_t, program, location, count, values);
}

GLboolean APIENTRY glIsTextureHandleResidentARB(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsTextureHandleResidentARB(*rvk_t, handle);
}

GLboolean APIENTRY glIsImageHandleResidentARB(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsImageHandleResidentARB(*rvk_t, handle);
}

void APIENTRY glVertexAttribL1ui64ARB(GLuint index, GLuint64EXT x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL1ui64ARB(*rvk_t, index, x);
}

void APIENTRY glVertexAttribL1ui64vARB(GLuint index, const GLuint64EXT* v) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribL1ui64vARB(*rvk_t, index, v);
}

void APIENTRY glGetVertexAttribLui64vARB(GLuint index, GLenum pname, GLuint64EXT* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexAttribLui64vARB(*rvk_t, index, pname, params);
}

GLuint64 APIENTRY glGetTextureHandleNV(GLuint texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetTextureHandleNV(*rvk_t, texture);
}

GLuint64 APIENTRY glGetTextureSamplerHandleNV(GLuint texture, GLuint sampler) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetTextureSamplerHandleNV(*rvk_t, texture, sampler);
}

void APIENTRY glMakeTextureHandleResidentNV(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeTextureHandleResidentNV(*rvk_t, handle);
}

void APIENTRY glMakeTextureHandleNonResidentNV(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeTextureHandleNonResidentNV(*rvk_t, handle);
}

GLuint64 APIENTRY glGetImageHandleNV(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glGetImageHandleNV(*rvk_t, texture, level, layered, layer, format);
}

void APIENTRY glMakeImageHandleResidentNV(GLuint64 handle, GLenum access) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeImageHandleResidentNV(*rvk_t, handle, access);
}

void APIENTRY glMakeImageHandleNonResidentNV(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeImageHandleNonResidentNV(*rvk_t, handle);
}

void APIENTRY glUniformHandleui64NV(GLint location, GLuint64 value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformHandleui64NV(*rvk_t, location, value);
}

void APIENTRY glUniformHandleui64vNV(GLint location, GLsizei count, const GLuint64* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformHandleui64vNV(*rvk_t, location, count, value);
}

void APIENTRY glProgramUniformHandleui64NV(GLuint program, GLint location, GLuint64 value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformHandleui64NV(*rvk_t, program, location, value);
}

void APIENTRY glProgramUniformHandleui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64* values) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformHandleui64vNV(*rvk_t, program, location, count, values);
}

GLboolean APIENTRY glIsTextureHandleResidentNV(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsTextureHandleResidentNV(*rvk_t, handle);
}

GLboolean APIENTRY glIsImageHandleResidentNV(GLuint64 handle) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsImageHandleResidentNV(*rvk_t, handle);
}

void APIENTRY glMakeBufferResidentNV(GLenum target, GLenum access) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeBufferResidentNV(*rvk_t, target, access);
}

void APIENTRY glMakeBufferNonResidentNV(GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeBufferNonResidentNV(*rvk_t, target);
}

GLboolean APIENTRY glIsBufferResidentNV(GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsBufferResidentNV(*rvk_t, target);
}

void APIENTRY glMakeNamedBufferResidentNV(GLuint buffer, GLenum access) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeNamedBufferResidentNV(*rvk_t, buffer, access);
}

void APIENTRY glMakeNamedBufferNonResidentNV(GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMakeNamedBufferNonResidentNV(*rvk_t, buffer);
}

GLboolean APIENTRY glIsNamedBufferResidentNV(GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsNamedBufferResidentNV(*rvk_t, buffer);
}

void APIENTRY glGetBufferParameterui64vNV(GLenum target, GLenum pname, GLuint64EXT* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetBufferParameterui64vNV(*rvk_t, target, pname, params);
}

void APIENTRY glGetNamedBufferParameterui64vNV(GLuint buffer, GLenum pname, GLuint64EXT* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedBufferParameterui64vNV(*rvk_t, buffer, pname, params);
}

void APIENTRY glGetIntegerui64vNV(GLenum value, GLuint64EXT* result) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetIntegerui64vNV(*rvk_t, value, result);
}

void APIENTRY glUniformui64NV(GLint location, GLuint64EXT value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformui64NV(*rvk_t, location, value);
}

void APIENTRY glUniformui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniformui64vNV(*rvk_t, location, count, value);
}

void APIENTRY glGetUniformui64vNV(GLuint program, GLint location, GLuint64EXT* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformui64vNV(*rvk_t, program, location, params);
}

void APIENTRY glProgramUniformui64NV(GLuint program, GLint location, GLuint64EXT value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformui64NV(*rvk_t, program, location, value);
}

void APIENTRY glProgramUniformui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformui64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glBufferAddressRangeNV(GLenum pname, GLuint index, GLuint64EXT address, GLsizeiptr length) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBufferAddressRangeNV(*rvk_t, pname, index, address, length);
}

void APIENTRY glVertexFormatNV(GLint size, GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexFormatNV(*rvk_t, size, type, stride);
}

void APIENTRY glNormalFormatNV(GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNormalFormatNV(*rvk_t, type, stride);
}

void APIENTRY glColorFormatNV(GLint size, GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glColorFormatNV(*rvk_t, size, type, stride);
}

void APIENTRY glIndexFormatNV(GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glIndexFormatNV(*rvk_t, type, stride);
}

void APIENTRY glTexCoordFormatNV(GLint size, GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTexCoordFormatNV(*rvk_t, size, type, stride);
}

void APIENTRY glEdgeFlagFormatNV(GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEdgeFlagFormatNV(*rvk_t, stride);
}

void APIENTRY glSecondaryColorFormatNV(GLint size, GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glSecondaryColorFormatNV(*rvk_t, size, type, stride);
}

void APIENTRY glFogCoordFormatNV(GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFogCoordFormatNV(*rvk_t, type, stride);
}

void APIENTRY glVertexAttribFormatNV(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribFormatNV(*rvk_t, index, size, type, normalized, stride);
}

void APIENTRY glVertexAttribIFormatNV(GLuint index, GLint size, GLenum type, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexAttribIFormatNV(*rvk_t, index, size, type, stride);
}

void APIENTRY glGetIntegerui64i_vNV(GLenum value, GLuint index, GLuint64EXT* result) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetIntegerui64i_vNV(*rvk_t, value, index, result);
}

void APIENTRY glUniform1i64NV(GLint location, GLint64EXT x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1i64NV(*rvk_t, location, x);
}

void APIENTRY glUniform2i64NV(GLint location, GLint64EXT x, GLint64EXT y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2i64NV(*rvk_t, location, x, y);
}

void APIENTRY glUniform3i64NV(GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3i64NV(*rvk_t, location, x, y, z);
}

void APIENTRY glUniform4i64NV(GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4i64NV(*rvk_t, location, x, y, z, w);
}

void APIENTRY glUniform1i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1i64vNV(*rvk_t, location, count, value);
}

void APIENTRY glUniform2i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2i64vNV(*rvk_t, location, count, value);
}

void APIENTRY glUniform3i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3i64vNV(*rvk_t, location, count, value);
}

void APIENTRY glUniform4i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4i64vNV(*rvk_t, location, count, value);
}

void APIENTRY glUniform1ui64NV(GLint location, GLuint64EXT x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1ui64NV(*rvk_t, location, x);
}

void APIENTRY glUniform2ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2ui64NV(*rvk_t, location, x, y);
}

void APIENTRY glUniform3ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3ui64NV(*rvk_t, location, x, y, z);
}

void APIENTRY glUniform4ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4ui64NV(*rvk_t, location, x, y, z, w);
}

void APIENTRY glUniform1ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform1ui64vNV(*rvk_t, location, count, value);
}

void APIENTRY glUniform2ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform2ui64vNV(*rvk_t, location, count, value);
}

void APIENTRY glUniform3ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform3ui64vNV(*rvk_t, location, count, value);
}

void APIENTRY glUniform4ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glUniform4ui64vNV(*rvk_t, location, count, value);
}

void APIENTRY glGetUniformi64vNV(GLuint program, GLint location, GLint64EXT* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetUniformi64vNV(*rvk_t, program, location, params);
}

void APIENTRY glProgramUniform1i64NV(GLuint program, GLint location, GLint64EXT x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1i64NV(*rvk_t, program, location, x);
}

void APIENTRY glProgramUniform2i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2i64NV(*rvk_t, program, location, x, y);
}

void APIENTRY glProgramUniform3i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3i64NV(*rvk_t, program, location, x, y, z);
}

void APIENTRY glProgramUniform4i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4i64NV(*rvk_t, program, location, x, y, z, w);
}

void APIENTRY glProgramUniform1i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1i64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2i64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3i64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4i64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform1ui64NV(GLuint program, GLint location, GLuint64EXT x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1ui64NV(*rvk_t, program, location, x);
}

void APIENTRY glProgramUniform2ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2ui64NV(*rvk_t, program, location, x, y);
}

void APIENTRY glProgramUniform3ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3ui64NV(*rvk_t, program, location, x, y, z);
}

void APIENTRY glProgramUniform4ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4ui64NV(*rvk_t, program, location, x, y, z, w);
}

void APIENTRY glProgramUniform1ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1ui64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2ui64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3ui64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4ui64vNV(*rvk_t, program, location, count, value);
}

void APIENTRY glMatrixLoadfEXT(GLenum mode, const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixLoadfEXT(*rvk_t, mode, m);
}

void APIENTRY glMatrixLoaddEXT(GLenum mode, const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixLoaddEXT(*rvk_t, mode, m);
}

void APIENTRY glMatrixMultfEXT(GLenum mode, const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixMultfEXT(*rvk_t, mode, m);
}

void APIENTRY glMatrixMultdEXT(GLenum mode, const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixMultdEXT(*rvk_t, mode, m);
}

void APIENTRY glMatrixLoadIdentityEXT(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixLoadIdentityEXT(*rvk_t, mode);
}

void APIENTRY glMatrixRotatefEXT(GLenum mode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixRotatefEXT(*rvk_t, mode, angle, x, y, z);
}

void APIENTRY glMatrixRotatedEXT(GLenum mode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixRotatedEXT(*rvk_t, mode, angle, x, y, z);
}

void APIENTRY glMatrixScalefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixScalefEXT(*rvk_t, mode, x, y, z);
}

void APIENTRY glMatrixScaledEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixScaledEXT(*rvk_t, mode, x, y, z);
}

void APIENTRY glMatrixTranslatefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixTranslatefEXT(*rvk_t, mode, x, y, z);
}

void APIENTRY glMatrixTranslatedEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixTranslatedEXT(*rvk_t, mode, x, y, z);
}

void APIENTRY glMatrixFrustumEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixFrustumEXT(*rvk_t, mode, left, right, bottom, top, zNear, zFar);
}

void APIENTRY glMatrixOrthoEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixOrthoEXT(*rvk_t, mode, left, right, bottom, top, zNear, zFar);
}

void APIENTRY glMatrixPopEXT(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixPopEXT(*rvk_t, mode);
}

void APIENTRY glMatrixPushEXT(GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixPushEXT(*rvk_t, mode);
}

void APIENTRY glClientAttribDefaultEXT(GLbitfield mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glClientAttribDefaultEXT(*rvk_t, mask);
}

void APIENTRY glPushClientAttribDefaultEXT(GLbitfield mask) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glPushClientAttribDefaultEXT(*rvk_t, mask);
}

void APIENTRY glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterfEXT(*rvk_t, texture, target, pname, param);
}

void APIENTRY glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterfvEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameteriEXT(*rvk_t, texture, target, pname, param);
}

void APIENTRY glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterivEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureImage1DEXT(*rvk_t, texture, target, level, internalformat, width, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, 1, 1, false), rvk_pixels_offset);
}

void APIENTRY glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureImage2DEXT(*rvk_t, texture, target, level, internalformat, width, height, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureSubImage1DEXT(*rvk_t, texture, target, level, xoffset, width, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, 1, 1, false), rvk_pixels_offset);
}

void APIENTRY glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureSubImage2DEXT(*rvk_t, texture, target, level, xoffset, yoffset, width, height, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glCopyTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureImage1DEXT(*rvk_t, texture, target, level, internalformat, x, y, width, border);
}

void APIENTRY glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureImage2DEXT(*rvk_t, texture, target, level, internalformat, x, y, width, height, border);
}

void APIENTRY glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureSubImage1DEXT(*rvk_t, texture, target, level, xoffset, x, y, width);
}

void APIENTRY glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureSubImage2DEXT(*rvk_t, texture, target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetTextureImageEXT(*rvk_t, texture, target, level, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::level_bytes(target, level, format, type), rvk_pixels_offset);
}

void APIENTRY glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameterfvEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameterivEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureLevelParameterfvEXT(*rvk_t, texture, target, level, pname, params);
}

void APIENTRY glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureLevelParameterivEXT(*rvk_t, texture, target, level, pname, params);
}

void APIENTRY glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureImage3DEXT(*rvk_t, texture, target, level, internalformat, width, height, depth, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_pixels_offset);
}

void APIENTRY glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glTextureSubImage3DEXT(*rvk_t, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_pixels_offset);
}

void APIENTRY glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyTextureSubImage3DEXT(*rvk_t, texture, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture) {
    auto* rvk_t = ::rvk::glicd::wire();
    ::rvk::glicd::forget_bindings();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glBindMultiTextureEXT(*rvk_t, texunit, target, texture);
}

void APIENTRY glMultiTexCoordPointerEXT(GLenum texunit, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexCoordPointerEXT(*rvk_t, texunit, size, type, stride, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)));
}

void APIENTRY glMultiTexEnvfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexEnvfEXT(*rvk_t, texunit, target, pname, param);
}

void APIENTRY glMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexEnvfvEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glMultiTexEnviEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexEnviEXT(*rvk_t, texunit, target, pname, param);
}

void APIENTRY glMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexEnvivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glMultiTexGendEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexGendEXT(*rvk_t, texunit, coord, pname, param);
}

void APIENTRY glMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexGendvEXT(*rvk_t, texunit, coord, pname, params);
}

void APIENTRY glMultiTexGenfEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexGenfEXT(*rvk_t, texunit, coord, pname, param);
}

void APIENTRY glMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexGenfvEXT(*rvk_t, texunit, coord, pname, params);
}

void APIENTRY glMultiTexGeniEXT(GLenum texunit, GLenum coord, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexGeniEXT(*rvk_t, texunit, coord, pname, param);
}

void APIENTRY glMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexGenivEXT(*rvk_t, texunit, coord, pname, params);
}

void APIENTRY glGetMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexEnvfvEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glGetMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexEnvivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glGetMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexGendvEXT(*rvk_t, texunit, coord, pname, params);
}

void APIENTRY glGetMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexGenfvEXT(*rvk_t, texunit, coord, pname, params);
}

void APIENTRY glGetMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexGenivEXT(*rvk_t, texunit, coord, pname, params);
}

void APIENTRY glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexParameteriEXT(*rvk_t, texunit, target, pname, param);
}

void APIENTRY glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexParameterivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexParameterfEXT(*rvk_t, texunit, target, pname, param);
}

void APIENTRY glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexParameterfvEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glMultiTexImage1DEXT(*rvk_t, texunit, target, level, internalformat, width, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, 1, 1, false), rvk_pixels_offset);
}

void APIENTRY glMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glMultiTexImage2DEXT(*rvk_t, texunit, target, level, internalformat, width, height, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glMultiTexSubImage1DEXT(*rvk_t, texunit, target, level, xoffset, width, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, 1, 1, false), rvk_pixels_offset);
}

void APIENTRY glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glMultiTexSubImage2DEXT(*rvk_t, texunit, target, level, xoffset, yoffset, width, height, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, 1, false), rvk_pixels_offset);
}

void APIENTRY glCopyMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyMultiTexImage1DEXT(*rvk_t, texunit, target, level, internalformat, x, y, width, border);
}

void APIENTRY glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyMultiTexImage2DEXT(*rvk_t, texunit, target, level, internalformat, x, y, width, height, border);
}

void APIENTRY glCopyMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyMultiTexSubImage1DEXT(*rvk_t, texunit, target, level, xoffset, x, y, width);
}

void APIENTRY glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyMultiTexSubImage2DEXT(*rvk_t, texunit, target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetMultiTexImageEXT(*rvk_t, texunit, target, level, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::level_bytes(target, level, format, type), rvk_pixels_offset);
}

void APIENTRY glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexParameterfvEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexParameterivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexLevelParameterfvEXT(*rvk_t, texunit, target, level, pname, params);
}

void APIENTRY glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexLevelParameterivEXT(*rvk_t, texunit, target, level, pname, params);
}

void APIENTRY glMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glMultiTexImage3DEXT(*rvk_t, texunit, target, level, internalformat, width, height, depth, border, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_pixels_offset);
}

void APIENTRY glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_pixels_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glMultiTexSubImage3DEXT(*rvk_t, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels, rvk_pixels_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, width, height, depth, false), rvk_pixels_offset);
}

void APIENTRY glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCopyMultiTexSubImage3DEXT(*rvk_t, texunit, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY glEnableClientStateIndexedEXT(GLenum array, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnableClientStateIndexedEXT(*rvk_t, array, index);
}

void APIENTRY glDisableClientStateIndexedEXT(GLenum array, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisableClientStateIndexedEXT(*rvk_t, array, index);
}

void APIENTRY glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetFloatIndexedvEXT(*rvk_t, target, index, data);
}

void APIENTRY glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetDoubleIndexedvEXT(*rvk_t, target, index, data);
}

void APIENTRY glEnableIndexedEXT(GLenum target, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnableIndexedEXT(*rvk_t, target, index);
}

void APIENTRY glDisableIndexedEXT(GLenum target, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisableIndexedEXT(*rvk_t, target, index);
}

GLboolean APIENTRY glIsEnabledIndexedEXT(GLenum target, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glIsEnabledIndexedEXT(*rvk_t, target, index);
}

void APIENTRY glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetIntegerIndexedvEXT(*rvk_t, target, index, data);
}

void APIENTRY glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetBooleanIndexedvEXT(*rvk_t, target, index, data);
}

void APIENTRY glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedTextureImage3DEXT(*rvk_t, texture, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void APIENTRY glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedTextureImage2DEXT(*rvk_t, texture, target, level, internalformat, width, height, border, imageSize, bits);
}

void APIENTRY glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedTextureImage1DEXT(*rvk_t, texture, target, level, internalformat, width, border, imageSize, bits);
}

void APIENTRY glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedTextureSubImage3DEXT(*rvk_t, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void APIENTRY glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedTextureSubImage2DEXT(*rvk_t, texture, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void APIENTRY glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedTextureSubImage1DEXT(*rvk_t, texture, target, level, xoffset, width, format, imageSize, bits);
}

void APIENTRY glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint lod, void* img) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_img_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetCompressedTextureImageEXT(*rvk_t, texture, target, lod, img, rvk_img_offset ? 0 : ::rvk::glicd::compressed_level_bytes(target, lod, true), rvk_img_offset);
}

void APIENTRY glCompressedMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedMultiTexImage3DEXT(*rvk_t, texunit, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void APIENTRY glCompressedMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedMultiTexImage2DEXT(*rvk_t, texunit, target, level, internalformat, width, height, border, imageSize, bits);
}

void APIENTRY glCompressedMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedMultiTexImage1DEXT(*rvk_t, texunit, target, level, internalformat, width, border, imageSize, bits);
}

void APIENTRY glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedMultiTexSubImage3DEXT(*rvk_t, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void APIENTRY glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedMultiTexSubImage2DEXT(*rvk_t, texunit, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void APIENTRY glCompressedMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* bits) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glCompressedMultiTexSubImage1DEXT(*rvk_t, texunit, target, level, xoffset, width, format, imageSize, bits);
}

void APIENTRY glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint lod, void* img) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_img_offset = ::rvk::glicd::pixel_buffer_bound(true);
    ::av::glproto::rvk_glGetCompressedMultiTexImageEXT(*rvk_t, texunit, target, lod, img, rvk_img_offset ? 0 : ::rvk::glicd::compressed_level_bytes(target, lod, true), rvk_img_offset);
}

void APIENTRY glMatrixLoadTransposefEXT(GLenum mode, const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixLoadTransposefEXT(*rvk_t, mode, m);
}

void APIENTRY glMatrixLoadTransposedEXT(GLenum mode, const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixLoadTransposedEXT(*rvk_t, mode, m);
}

void APIENTRY glMatrixMultTransposefEXT(GLenum mode, const GLfloat* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixMultTransposefEXT(*rvk_t, mode, m);
}

void APIENTRY glMatrixMultTransposedEXT(GLenum mode, const GLdouble* m) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMatrixMultTransposedEXT(*rvk_t, mode, m);
}

void APIENTRY glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedBufferDataEXT(*rvk_t, buffer, size, data, usage);
}

void APIENTRY glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedBufferSubDataEXT(*rvk_t, buffer, offset, size, data);
}

void APIENTRY glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedBufferParameterivEXT(*rvk_t, buffer, pname, params);
}

void APIENTRY glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedBufferSubDataEXT(*rvk_t, buffer, offset, size, data);
}

void APIENTRY glProgramUniform1fEXT(GLuint program, GLint location, GLfloat v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1fEXT(*rvk_t, program, location, v0);
}

void APIENTRY glProgramUniform2fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2fEXT(*rvk_t, program, location, v0, v1);
}

void APIENTRY glProgramUniform3fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3fEXT(*rvk_t, program, location, v0, v1, v2);
}

void APIENTRY glProgramUniform4fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4fEXT(*rvk_t, program, location, v0, v1, v2, v3);
}

void APIENTRY glProgramUniform1iEXT(GLuint program, GLint location, GLint v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1iEXT(*rvk_t, program, location, v0);
}

void APIENTRY glProgramUniform2iEXT(GLuint program, GLint location, GLint v0, GLint v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2iEXT(*rvk_t, program, location, v0, v1);
}

void APIENTRY glProgramUniform3iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3iEXT(*rvk_t, program, location, v0, v1, v2);
}

void APIENTRY glProgramUniform4iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4iEXT(*rvk_t, program, location, v0, v1, v2, v3);
}

void APIENTRY glProgramUniform1fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1fvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2fvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3fvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4fvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform1ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1ivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2ivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3ivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4ivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniformMatrix2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x3fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x2fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x4fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x2fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x4fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x3fvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureBufferEXT(*rvk_t, texture, target, internalformat, buffer);
}

void APIENTRY glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexBufferEXT(*rvk_t, texunit, target, internalformat, buffer);
}

void APIENTRY glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterIivEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, const GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureParameterIuivEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameterIivEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetTextureParameterIuivEXT(*rvk_t, texture, target, pname, params);
}

void APIENTRY glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexParameterIivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, const GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexParameterIuivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexParameterIivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetMultiTexParameterIuivEXT(*rvk_t, texunit, target, pname, params);
}

void APIENTRY glProgramUniform1uiEXT(GLuint program, GLint location, GLuint v0) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1uiEXT(*rvk_t, program, location, v0);
}

void APIENTRY glProgramUniform2uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2uiEXT(*rvk_t, program, location, v0, v1);
}

void APIENTRY glProgramUniform3uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3uiEXT(*rvk_t, program, location, v0, v1, v2);
}

void APIENTRY glProgramUniform4uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4uiEXT(*rvk_t, program, location, v0, v1, v2, v3);
}

void APIENTRY glProgramUniform1uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1uivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2uivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3uivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4uivEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glNamedProgramLocalParameters4fvEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameters4fvEXT(*rvk_t, program, target, index, count, params);
}

void APIENTRY glNamedProgramLocalParameterI4iEXT(GLuint program, GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameterI4iEXT(*rvk_t, program, target, index, x, y, z, w);
}

void APIENTRY glNamedProgramLocalParameterI4ivEXT(GLuint program, GLenum target, GLuint index, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameterI4ivEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glNamedProgramLocalParametersI4ivEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParametersI4ivEXT(*rvk_t, program, target, index, count, params);
}

void APIENTRY glNamedProgramLocalParameterI4uiEXT(GLuint program, GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameterI4uiEXT(*rvk_t, program, target, index, x, y, z, w);
}

void APIENTRY glNamedProgramLocalParameterI4uivEXT(GLuint program, GLenum target, GLuint index, const GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameterI4uivEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glNamedProgramLocalParametersI4uivEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParametersI4uivEXT(*rvk_t, program, target, index, count, params);
}

void APIENTRY glGetNamedProgramLocalParameterIivEXT(GLuint program, GLenum target, GLuint index, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedProgramLocalParameterIivEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glGetNamedProgramLocalParameterIuivEXT(GLuint program, GLenum target, GLuint index, GLuint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedProgramLocalParameterIuivEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glEnableClientStateiEXT(GLenum array, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnableClientStateiEXT(*rvk_t, array, index);
}

void APIENTRY glDisableClientStateiEXT(GLenum array, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisableClientStateiEXT(*rvk_t, array, index);
}

void APIENTRY glGetFloati_vEXT(GLenum pname, GLuint index, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetFloati_vEXT(*rvk_t, pname, index, params);
}

void APIENTRY glGetDoublei_vEXT(GLenum pname, GLuint index, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetDoublei_vEXT(*rvk_t, pname, index, params);
}

void APIENTRY glNamedProgramStringEXT(GLuint program, GLenum target, GLenum format, GLsizei len, const void* string) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramStringEXT(*rvk_t, program, target, format, len, string);
}

void APIENTRY glNamedProgramLocalParameter4dEXT(GLuint program, GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameter4dEXT(*rvk_t, program, target, index, x, y, z, w);
}

void APIENTRY glNamedProgramLocalParameter4dvEXT(GLuint program, GLenum target, GLuint index, const GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameter4dvEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glNamedProgramLocalParameter4fEXT(GLuint program, GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameter4fEXT(*rvk_t, program, target, index, x, y, z, w);
}

void APIENTRY glNamedProgramLocalParameter4fvEXT(GLuint program, GLenum target, GLuint index, const GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedProgramLocalParameter4fvEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glGetNamedProgramLocalParameterdvEXT(GLuint program, GLenum target, GLuint index, GLdouble* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedProgramLocalParameterdvEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glGetNamedProgramLocalParameterfvEXT(GLuint program, GLenum target, GLuint index, GLfloat* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedProgramLocalParameterfvEXT(*rvk_t, program, target, index, params);
}

void APIENTRY glGetNamedProgramivEXT(GLuint program, GLenum target, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedProgramivEXT(*rvk_t, program, target, pname, params);
}

void APIENTRY glGetNamedProgramStringEXT(GLuint program, GLenum target, GLenum pname, void* string) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedProgramStringEXT(*rvk_t, program, target, pname, string);
}

void APIENTRY glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedRenderbufferStorageEXT(*rvk_t, renderbuffer, internalformat, width, height);
}

void APIENTRY glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedRenderbufferParameterivEXT(*rvk_t, renderbuffer, pname, params);
}

void APIENTRY glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedRenderbufferStorageMultisampleEXT(*rvk_t, renderbuffer, samples, internalformat, width, height);
}

void APIENTRY glNamedRenderbufferStorageMultisampleCoverageEXT(GLuint renderbuffer, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedRenderbufferStorageMultisampleCoverageEXT(*rvk_t, renderbuffer, coverageSamples, colorSamples, internalformat, width, height);
}

GLenum APIENTRY glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return 0;
    return ::av::glproto::rvk_glCheckNamedFramebufferStatusEXT(*rvk_t, framebuffer, target);
}

void APIENTRY glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTexture1DEXT(*rvk_t, framebuffer, attachment, textarget, texture, level);
}

void APIENTRY glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTexture2DEXT(*rvk_t, framebuffer, attachment, textarget, texture, level);
}

void APIENTRY glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTexture3DEXT(*rvk_t, framebuffer, attachment, textarget, texture, level, zoffset);
}

void APIENTRY glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferRenderbufferEXT(*rvk_t, framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void APIENTRY glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedFramebufferAttachmentParameterivEXT(*rvk_t, framebuffer, attachment, pname, params);
}

void APIENTRY glGenerateTextureMipmapEXT(GLuint texture, GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenerateTextureMipmapEXT(*rvk_t, texture, target);
}

void APIENTRY glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGenerateMultiTexMipmapEXT(*rvk_t, texunit, target);
}

void APIENTRY glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferDrawBufferEXT(*rvk_t, framebuffer, mode);
}

void APIENTRY glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferDrawBuffersEXT(*rvk_t, framebuffer, n, bufs);
}

void APIENTRY glFramebufferReadBufferEXT(GLuint framebuffer, GLenum mode) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glFramebufferReadBufferEXT(*rvk_t, framebuffer, mode);
}

void APIENTRY glGetFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetFramebufferParameterivEXT(*rvk_t, framebuffer, pname, params);
}

void APIENTRY glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedCopyBufferSubDataEXT(*rvk_t, readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void APIENTRY glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTextureEXT(*rvk_t, framebuffer, attachment, texture, level);
}

void APIENTRY glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTextureLayerEXT(*rvk_t, framebuffer, attachment, texture, level, layer);
}

void APIENTRY glNamedFramebufferTextureFaceEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLenum face) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferTextureFaceEXT(*rvk_t, framebuffer, attachment, texture, level, face);
}

void APIENTRY glTextureRenderbufferEXT(GLuint texture, GLenum target, GLuint renderbuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureRenderbufferEXT(*rvk_t, texture, target, renderbuffer);
}

void APIENTRY glMultiTexRenderbufferEXT(GLenum texunit, GLenum target, GLuint renderbuffer) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glMultiTexRenderbufferEXT(*rvk_t, texunit, target, renderbuffer);
}

void APIENTRY glVertexArrayVertexOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexOffsetEXT(*rvk_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY glVertexArrayColorOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayColorOffsetEXT(*rvk_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY glVertexArrayEdgeFlagOffsetEXT(GLuint vaobj, GLuint buffer, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayEdgeFlagOffsetEXT(*rvk_t, vaobj, buffer, stride, offset);
}

void APIENTRY glVertexArrayIndexOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayIndexOffsetEXT(*rvk_t, vaobj, buffer, type, stride, offset);
}

void APIENTRY glVertexArrayNormalOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayNormalOffsetEXT(*rvk_t, vaobj, buffer, type, stride, offset);
}

void APIENTRY glVertexArrayTexCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayTexCoordOffsetEXT(*rvk_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY glVertexArrayMultiTexCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLenum texunit, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayMultiTexCoordOffsetEXT(*rvk_t, vaobj, buffer, texunit, size, type, stride, offset);
}

void APIENTRY glVertexArrayFogCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayFogCoordOffsetEXT(*rvk_t, vaobj, buffer, type, stride, offset);
}

void APIENTRY glVertexArraySecondaryColorOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArraySecondaryColorOffsetEXT(*rvk_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribOffsetEXT(*rvk_t, vaobj, buffer, index, size, type, normalized, stride, offset);
}

void APIENTRY glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribIOffsetEXT(*rvk_t, vaobj, buffer, index, size, type, stride, offset);
}

void APIENTRY glEnableVertexArrayEXT(GLuint vaobj, GLenum array) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnableVertexArrayEXT(*rvk_t, vaobj, array);
}

void APIENTRY glDisableVertexArrayEXT(GLuint vaobj, GLenum array) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisableVertexArrayEXT(*rvk_t, vaobj, array);
}

void APIENTRY glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glEnableVertexArrayAttribEXT(*rvk_t, vaobj, index);
}

void APIENTRY glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glDisableVertexArrayAttribEXT(*rvk_t, vaobj, index);
}

void APIENTRY glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexArrayIntegervEXT(*rvk_t, vaobj, pname, param);
}

void APIENTRY glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, GLint* param) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetVertexArrayIntegeri_vEXT(*rvk_t, vaobj, index, pname, param);
}

void APIENTRY glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedBufferStorageEXT(*rvk_t, buffer, size, data, flags);
}

void APIENTRY glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearNamedBufferDataEXT(*rvk_t, buffer, internalformat, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, 1, 1, 1, false), rvk_data_offset);
}

void APIENTRY glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLsizeiptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    const bool rvk_data_offset = ::rvk::glicd::pixel_buffer_bound(false);
    ::av::glproto::rvk_glClearNamedBufferSubDataEXT(*rvk_t, buffer, internalformat, offset, size, format, type, data, rvk_data_offset ? 0 : ::rvk::glicd::pixel_bytes(format, type, 1, 1, 1, false), rvk_data_offset);
}

void APIENTRY glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glNamedFramebufferParameteriEXT(*rvk_t, framebuffer, pname, param);
}

void APIENTRY glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint* params) {
    auto* rvk_t = ::rvk::glicd::wire();
    framebuffer = ::rvk::glicd::framebuffer_name(framebuffer);
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glGetNamedFramebufferParameterivEXT(*rvk_t, framebuffer, pname, params);
}

void APIENTRY glProgramUniform1dEXT(GLuint program, GLint location, GLdouble x) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1dEXT(*rvk_t, program, location, x);
}

void APIENTRY glProgramUniform2dEXT(GLuint program, GLint location, GLdouble x, GLdouble y) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2dEXT(*rvk_t, program, location, x, y);
}

void APIENTRY glProgramUniform3dEXT(GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3dEXT(*rvk_t, program, location, x, y, z);
}

void APIENTRY glProgramUniform4dEXT(GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4dEXT(*rvk_t, program, location, x, y, z, w);
}

void APIENTRY glProgramUniform1dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform1dvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform2dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform2dvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform3dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform3dvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniform4dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniform4dvEXT(*rvk_t, program, location, count, value);
}

void APIENTRY glProgramUniformMatrix2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x3dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix2x4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix2x4dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x2dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix3x4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix3x4dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x2dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glProgramUniformMatrix4x3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glProgramUniformMatrix4x3dvEXT(*rvk_t, program, location, count, transpose, value);
}

void APIENTRY glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureBufferRangeEXT(*rvk_t, texture, target, internalformat, buffer, offset, size);
}

void APIENTRY glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage1DEXT(*rvk_t, texture, target, levels, internalformat, width);
}

void APIENTRY glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage2DEXT(*rvk_t, texture, target, levels, internalformat, width, height);
}

void APIENTRY glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage3DEXT(*rvk_t, texture, target, levels, internalformat, width, height, depth);
}

void APIENTRY glTextureStorage2DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage2DMultisampleEXT(*rvk_t, texture, target, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glTextureStorage3DMultisampleEXT(*rvk_t, texture, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayBindVertexBufferEXT(*rvk_t, vaobj, bindingindex, buffer, offset, stride);
}

void APIENTRY glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribFormatEXT(*rvk_t, vaobj, attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribIFormatEXT(*rvk_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribLFormatEXT(*rvk_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribBindingEXT(*rvk_t, vaobj, attribindex, bindingindex);
}

void APIENTRY glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexBindingDivisorEXT(*rvk_t, vaobj, bindingindex, divisor);
}

void APIENTRY glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribLOffsetEXT(*rvk_t, vaobj, buffer, index, size, type, stride, offset);
}

void APIENTRY glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor) {
    auto* rvk_t = ::rvk::glicd::wire();
    if (rvk_t == nullptr) return;
    ::av::glproto::rvk_glVertexArrayVertexAttribDivisorEXT(*rvk_t, vaobj, index, divisor);
}

}  // extern "C"
