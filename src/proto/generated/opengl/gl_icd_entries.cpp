// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opengl --xml gl.xml --wgl wgl.xml
//                  --profile tools/codegen/profiles/opengl.toml --out ...

#include "gl_icd_entries.h"

#include <stdint.h>

#include "core/trace/function_scope.hpp"
#include "proto/client/gl_icd_support.hpp"
#include "gl_dispatch_client.h"
#include "gl_helpers.h"

// Every entry point whose whole job is to forward.
namespace av::glicd::generated {

void APIENTRY Wrap_glActiveShaderProgram(GLuint pipeline, GLuint program) {
    AV_SCOPE("glActiveShaderProgram");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glActiveShaderProgram(*av_t, pipeline, program);
}

void APIENTRY Wrap_glActiveTexture(GLenum texture) {
    AV_SCOPE("glActiveTexture");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::skip_active_texture(texture)) return;
    ::av::glproto::av_glActiveTexture(*av_t, texture);
}

void APIENTRY Wrap_glAttachShader(GLuint program, GLuint shader) {
    AV_SCOPE("glAttachShader");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glAttachShader(*av_t, program, shader);
}

void APIENTRY Wrap_glBeginConditionalRender(GLuint id, GLenum mode) {
    AV_SCOPE("glBeginConditionalRender");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBeginConditionalRender(*av_t, id, mode);
}

void APIENTRY Wrap_glBeginQuery(GLenum target, GLuint id) {
    AV_SCOPE("glBeginQuery");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBeginQuery(*av_t, target, id);
}

void APIENTRY Wrap_glBeginQueryIndexed(GLenum target, GLuint index, GLuint id) {
    AV_SCOPE("glBeginQueryIndexed");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBeginQueryIndexed(*av_t, target, index, id);
}

void APIENTRY Wrap_glBeginTransformFeedback(GLenum primitiveMode) {
    AV_SCOPE("glBeginTransformFeedback");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glBeginTransformFeedback(*av_t, primitiveMode);
}

void APIENTRY Wrap_glBindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
    AV_SCOPE("glBindAttribLocation");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindAttribLocation(*av_t, program, index, name);
}

void APIENTRY Wrap_glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    AV_SCOPE("glBindBufferBase");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::bind_buffer_indexed(target, buffer);
    ::av::glproto::av_glBindBufferBase(*av_t, target, index, buffer);
}

void APIENTRY Wrap_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    AV_SCOPE("glBindBufferRange");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::bind_buffer_indexed(target, buffer);
    ::av::glproto::av_glBindBufferRange(*av_t, target, index, buffer, offset, size);
}

void APIENTRY Wrap_glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint* buffers) {
    AV_SCOPE("glBindBuffersBase");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::bind_buffers_indexed(target);
    ::av::glproto::av_glBindBuffersBase(*av_t, target, first, count, buffers);
}

void APIENTRY Wrap_glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizeiptr* sizes) {
    AV_SCOPE("glBindBuffersRange");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::bind_buffers_indexed(target);
    ::av::glproto::av_glBindBuffersRange(*av_t, target, first, count, buffers, offsets, sizes);
}

void APIENTRY Wrap_glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name) {
    AV_SCOPE("glBindFragDataLocation");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindFragDataLocation(*av_t, program, color, name);
}

void APIENTRY Wrap_glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index, const GLchar* name) {
    AV_SCOPE("glBindFragDataLocationIndexed");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindFragDataLocationIndexed(*av_t, program, colorNumber, index, name);
}

void APIENTRY Wrap_glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) {
    AV_SCOPE("glBindImageTexture");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glBindImageTexture(*av_t, unit, texture, level, layered, layer, access, format);
}

void APIENTRY Wrap_glBindImageTextures(GLuint first, GLsizei count, const GLuint* textures) {
    AV_SCOPE("glBindImageTextures");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glBindImageTextures(*av_t, first, count, textures);
}

void APIENTRY Wrap_glBindProgramPipeline(GLuint pipeline) {
    AV_SCOPE("glBindProgramPipeline");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindProgramPipeline(*av_t, pipeline);
}

void APIENTRY Wrap_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    AV_SCOPE("glBindRenderbuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindRenderbuffer(*av_t, target, renderbuffer);
}

void APIENTRY Wrap_glBindSampler(GLuint unit, GLuint sampler) {
    AV_SCOPE("glBindSampler");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::skip_bind_sampler(unit, sampler)) return;
    ::av::glproto::av_glBindSampler(*av_t, unit, sampler);
}

void APIENTRY Wrap_glBindSamplers(GLuint first, GLsizei count, const GLuint* samplers) {
    AV_SCOPE("glBindSamplers");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindSamplers(*av_t, first, count, samplers);
}

void APIENTRY Wrap_glBindTexture(GLenum target, GLuint texture) {
    AV_SCOPE("glBindTexture");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::skip_bind_texture(target, texture)) return;
    ::av::glproto::av_glBindTexture(*av_t, target, texture);
}

void APIENTRY Wrap_glBindTextureUnit(GLuint unit, GLuint texture) {
    AV_SCOPE("glBindTextureUnit");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindTextureUnit(*av_t, unit, texture);
}

void APIENTRY Wrap_glBindTextures(GLuint first, GLsizei count, const GLuint* textures) {
    AV_SCOPE("glBindTextures");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindTextures(*av_t, first, count, textures);
}

void APIENTRY Wrap_glBindTransformFeedback(GLenum target, GLuint id) {
    AV_SCOPE("glBindTransformFeedback");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::bind_buffers_indexed(0x8C8Eu);
    ::av::glproto::av_glBindTransformFeedback(*av_t, target, id);
}

void APIENTRY Wrap_glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    AV_SCOPE("glBindVertexBuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindVertexBuffer(*av_t, bindingindex, buffer, offset, stride);
}

void APIENTRY Wrap_glBindVertexBuffers(GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides) {
    AV_SCOPE("glBindVertexBuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindVertexBuffers(*av_t, first, count, buffers, offsets, strides);
}

void APIENTRY Wrap_glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    AV_SCOPE("glBlendColor");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendColor(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glBlendEquation(GLenum mode) {
    AV_SCOPE("glBlendEquation");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendEquation(*av_t, mode);
}

void APIENTRY Wrap_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    AV_SCOPE("glBlendEquationSeparate");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendEquationSeparate(*av_t, modeRGB, modeAlpha);
}

void APIENTRY Wrap_glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    AV_SCOPE("glBlendEquationSeparatei");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendEquationSeparatei(*av_t, buf, modeRGB, modeAlpha);
}

void APIENTRY Wrap_glBlendEquationi(GLuint buf, GLenum mode) {
    AV_SCOPE("glBlendEquationi");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendEquationi(*av_t, buf, mode);
}

void APIENTRY Wrap_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    AV_SCOPE("glBlendFunc");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendFunc(*av_t, sfactor, dfactor);
}

void APIENTRY Wrap_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    AV_SCOPE("glBlendFuncSeparate");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendFuncSeparate(*av_t, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

void APIENTRY Wrap_glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    AV_SCOPE("glBlendFuncSeparatei");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendFuncSeparatei(*av_t, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void APIENTRY Wrap_glBlendFunci(GLuint buf, GLenum src, GLenum dst) {
    AV_SCOPE("glBlendFunci");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlendFunci(*av_t, buf, src, dst);
}

void APIENTRY Wrap_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    AV_SCOPE("glBlitFramebuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlitFramebuffer(*av_t, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void APIENTRY Wrap_glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    AV_SCOPE("glBlitNamedFramebuffer");
    auto* av_t = ::av::glicd::wire();
    readFramebuffer = ::av::glicd::framebuffer_name(readFramebuffer);
    drawFramebuffer = ::av::glicd::framebuffer_name(drawFramebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glBlitNamedFramebuffer(*av_t, readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

GLenum APIENTRY Wrap_glCheckFramebufferStatus(GLenum target) {
    AV_SCOPE("glCheckFramebufferStatus");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glCheckFramebufferStatus(*av_t, target);
}

GLenum APIENTRY Wrap_glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target) {
    AV_SCOPE("glCheckNamedFramebufferStatus");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glCheckNamedFramebufferStatus(*av_t, framebuffer, target);
}

void APIENTRY Wrap_glClampColor(GLenum target, GLenum clamp) {
    AV_SCOPE("glClampColor");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClampColor(*av_t, target, clamp);
}

void APIENTRY Wrap_glClear(GLbitfield mask) {
    AV_SCOPE("glClear");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClear(*av_t, mask);
}

void APIENTRY Wrap_glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearBufferData");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::clear_bound_buffer(target, internalformat, format, type, data)) return;
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, 1, 1, 1, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glicd::host_writes_target(target);
    ::av::glproto::av_glClearBufferData(*av_t, target, internalformat, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearBufferSubData");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::clear_bound_buffer_range(target, internalformat, offset, size, format, type, data)) return;
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, 1, 1, 1, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glicd::host_writes_target(target);
    ::av::glproto::av_glClearBufferSubData(*av_t, target, internalformat, offset, size, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    AV_SCOPE("glClearBufferfi");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearBufferfi(*av_t, buffer, drawbuffer, depth, stencil);
}

void APIENTRY Wrap_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) {
    AV_SCOPE("glClearBufferfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearBufferfv(*av_t, buffer, drawbuffer, value);
}

void APIENTRY Wrap_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) {
    AV_SCOPE("glClearBufferiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearBufferiv(*av_t, buffer, drawbuffer, value);
}

void APIENTRY Wrap_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value) {
    AV_SCOPE("glClearBufferuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearBufferuiv(*av_t, buffer, drawbuffer, value);
}

void APIENTRY Wrap_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    AV_SCOPE("glClearColor");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearColor(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glClearDepth(GLdouble depth) {
    AV_SCOPE("glClearDepth");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearDepth(*av_t, depth);
}

void APIENTRY Wrap_glClearDepthf(GLfloat d) {
    AV_SCOPE("glClearDepthf");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearDepthf(*av_t, d);
}

void APIENTRY Wrap_glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearNamedBufferData");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::clear_named_buffer(buffer, internalformat, format, type, data)) return;
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, 1, 1, 1, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glClearNamedBufferData(*av_t, buffer, internalformat, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearNamedBufferSubData");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::clear_named_buffer_range(buffer, internalformat, offset, size, format, type, data)) return;
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, 1, 1, 1, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glClearNamedBufferSubData(*av_t, buffer, internalformat, offset, size, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    AV_SCOPE("glClearNamedFramebufferfi");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearNamedFramebufferfi(*av_t, framebuffer, buffer, drawbuffer, depth, stencil);
}

void APIENTRY Wrap_glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat* value) {
    AV_SCOPE("glClearNamedFramebufferfv");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearNamedFramebufferfv(*av_t, framebuffer, buffer, drawbuffer, value);
}

void APIENTRY Wrap_glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint* value) {
    AV_SCOPE("glClearNamedFramebufferiv");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearNamedFramebufferiv(*av_t, framebuffer, buffer, drawbuffer, value);
}

void APIENTRY Wrap_glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint* value) {
    AV_SCOPE("glClearNamedFramebufferuiv");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearNamedFramebufferuiv(*av_t, framebuffer, buffer, drawbuffer, value);
}

void APIENTRY Wrap_glClearStencil(GLint s) {
    AV_SCOPE("glClearStencil");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearStencil(*av_t, s);
}

void APIENTRY Wrap_glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearTexImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, 1, 1, 1, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glClearTexImage(*av_t, texture, level, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearTexSubImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glClearTexSubImage(*av_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glClipControl(GLenum origin, GLenum depth) {
    AV_SCOPE("glClipControl");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClipControl(*av_t, origin, depth);
}

void APIENTRY Wrap_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    AV_SCOPE("glColorMask");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorMask(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    AV_SCOPE("glColorMaski");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorMaski(*av_t, index, r, g, b, a);
}

void APIENTRY Wrap_glColorP3ui(GLenum type, GLuint color) {
    AV_SCOPE("glColorP3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorP3ui(*av_t, type, color);
}

void APIENTRY Wrap_glColorP3uiv(GLenum type, const GLuint* color) {
    AV_SCOPE("glColorP3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorP3uiv(*av_t, type, color);
}

void APIENTRY Wrap_glColorP4ui(GLenum type, GLuint color) {
    AV_SCOPE("glColorP4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorP4ui(*av_t, type, color);
}

void APIENTRY Wrap_glColorP4uiv(GLenum type, const GLuint* color) {
    AV_SCOPE("glColorP4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorP4uiv(*av_t, type, color);
}

void APIENTRY Wrap_glCompileShader(GLuint shader) {
    AV_SCOPE("glCompileShader");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompileShader(*av_t, shader);
}

void APIENTRY Wrap_glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTexImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, 1, 1, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTexImage1D(*av_t, target, level, internalformat, width, border, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTexImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, height, 1, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTexImage2D(*av_t, target, level, internalformat, width, height, border, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTexImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, height, depth, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTexImage3D(*av_t, target, level, internalformat, width, height, depth, border, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTexSubImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, 1, 1, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTexSubImage1D(*av_t, target, level, xoffset, width, format, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTexSubImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, height, 1, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTexSubImage2D(*av_t, target, level, xoffset, yoffset, width, height, format, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTexSubImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, height, depth, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTexSubImage3D(*av_t, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTextureSubImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, 1, 1, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTextureSubImage1D(*av_t, texture, level, xoffset, width, format, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTextureSubImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, height, 1, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTextureSubImage2D(*av_t, texture, level, xoffset, yoffset, width, height, format, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data) {
    AV_SCOPE("glCompressedTextureSubImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::compressed_bytes(width, height, depth, imageSize, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glproto::av_glCompressedTextureSubImage3D(*av_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    AV_SCOPE("glCopyBufferSubData");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_target(writeTarget);
    ::av::glproto::av_glCopyBufferSubData(*av_t, readTarget, writeTarget, readOffset, writeOffset, size);
}

void APIENTRY Wrap_glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    AV_SCOPE("glCopyImageSubData");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyImageSubData(*av_t, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

void APIENTRY Wrap_glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    AV_SCOPE("glCopyNamedBufferSubData");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(writeBuffer);
    ::av::glproto::av_glCopyNamedBufferSubData(*av_t, readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void APIENTRY Wrap_glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    AV_SCOPE("glCopyTexImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTexImage1D(*av_t, target, level, internalformat, x, y, width, border);
}

void APIENTRY Wrap_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    AV_SCOPE("glCopyTexImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTexImage2D(*av_t, target, level, internalformat, x, y, width, height, border);
}

void APIENTRY Wrap_glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    AV_SCOPE("glCopyTexSubImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTexSubImage1D(*av_t, target, level, xoffset, x, y, width);
}

void APIENTRY Wrap_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyTexSubImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTexSubImage2D(*av_t, target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY Wrap_glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyTexSubImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTexSubImage3D(*av_t, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY Wrap_glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    AV_SCOPE("glCopyTextureSubImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureSubImage1D(*av_t, texture, level, xoffset, x, y, width);
}

void APIENTRY Wrap_glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyTextureSubImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureSubImage2D(*av_t, texture, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY Wrap_glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyTextureSubImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureSubImage3D(*av_t, texture, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY Wrap_glCreateBuffers(GLsizei n, GLuint* buffers) {
    AV_SCOPE("glCreateBuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateBuffers(*av_t, n, buffers);
}

void APIENTRY Wrap_glCreateFramebuffers(GLsizei n, GLuint* framebuffers) {
    AV_SCOPE("glCreateFramebuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateFramebuffers(*av_t, n, framebuffers);
}

GLuint APIENTRY Wrap_glCreateProgram(void) {
    AV_SCOPE("glCreateProgram");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glCreateProgram(*av_t);
}

void APIENTRY Wrap_glCreateProgramPipelines(GLsizei n, GLuint* pipelines) {
    AV_SCOPE("glCreateProgramPipelines");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateProgramPipelines(*av_t, n, pipelines);
}

void APIENTRY Wrap_glCreateQueries(GLenum target, GLsizei n, GLuint* ids) {
    AV_SCOPE("glCreateQueries");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateQueries(*av_t, target, n, ids);
}

void APIENTRY Wrap_glCreateRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    AV_SCOPE("glCreateRenderbuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateRenderbuffers(*av_t, n, renderbuffers);
}

void APIENTRY Wrap_glCreateSamplers(GLsizei n, GLuint* samplers) {
    AV_SCOPE("glCreateSamplers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateSamplers(*av_t, n, samplers);
}

GLuint APIENTRY Wrap_glCreateShader(GLenum type) {
    AV_SCOPE("glCreateShader");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glCreateShader(*av_t, type);
}

GLuint APIENTRY Wrap_glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar*const* strings) {
    AV_SCOPE("glCreateShaderProgramv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glCreateShaderProgramv(*av_t, type, count, strings);
}

void APIENTRY Wrap_glCreateTextures(GLenum target, GLsizei n, GLuint* textures) {
    AV_SCOPE("glCreateTextures");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateTextures(*av_t, target, n, textures);
}

void APIENTRY Wrap_glCreateTransformFeedbacks(GLsizei n, GLuint* ids) {
    AV_SCOPE("glCreateTransformFeedbacks");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateTransformFeedbacks(*av_t, n, ids);
}

void APIENTRY Wrap_glCreateVertexArrays(GLsizei n, GLuint* arrays) {
    AV_SCOPE("glCreateVertexArrays");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCreateVertexArrays(*av_t, n, arrays);
}

void APIENTRY Wrap_glCullFace(GLenum mode) {
    AV_SCOPE("glCullFace");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCullFace(*av_t, mode);
}

void APIENTRY Wrap_glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled) {
    AV_SCOPE("glDebugMessageControl");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDebugMessageControl(*av_t, source, type, severity, count, ids, enabled);
}

void APIENTRY Wrap_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf) {
    AV_SCOPE("glDebugMessageInsert");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDebugMessageInsert(*av_t, source, type, id, severity, length, buf);
}

void APIENTRY Wrap_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    AV_SCOPE("glDeleteFramebuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteFramebuffers(*av_t, n, framebuffers);
}

void APIENTRY Wrap_glDeleteProgramPipelines(GLsizei n, const GLuint* pipelines) {
    AV_SCOPE("glDeleteProgramPipelines");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteProgramPipelines(*av_t, n, pipelines);
}

void APIENTRY Wrap_glDeleteQueries(GLsizei n, const GLuint* ids) {
    AV_SCOPE("glDeleteQueries");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteQueries(*av_t, n, ids);
}

void APIENTRY Wrap_glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
    AV_SCOPE("glDeleteRenderbuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteRenderbuffers(*av_t, n, renderbuffers);
}

void APIENTRY Wrap_glDeleteSamplers(GLsizei count, const GLuint* samplers) {
    AV_SCOPE("glDeleteSamplers");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteSamplers(*av_t, count, samplers);
}

void APIENTRY Wrap_glDeleteShader(GLuint shader) {
    AV_SCOPE("glDeleteShader");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteShader(*av_t, shader);
}

void APIENTRY Wrap_glDeleteSync(GLsync sync) {
    AV_SCOPE("glDeleteSync");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteSync(*av_t, reinterpret_cast<uint64_t>(sync));
}

void APIENTRY Wrap_glDeleteTextures(GLsizei n, const GLuint* textures) {
    AV_SCOPE("glDeleteTextures");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteTextures(*av_t, n, textures);
}

void APIENTRY Wrap_glDepthFunc(GLenum func) {
    AV_SCOPE("glDepthFunc");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDepthFunc(*av_t, func);
}

void APIENTRY Wrap_glDepthMask(GLboolean flag) {
    AV_SCOPE("glDepthMask");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDepthMask(*av_t, flag);
}

void APIENTRY Wrap_glDepthRange(GLdouble n, GLdouble f) {
    AV_SCOPE("glDepthRange");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDepthRange(*av_t, n, f);
}

void APIENTRY Wrap_glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble* v) {
    AV_SCOPE("glDepthRangeArrayv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDepthRangeArrayv(*av_t, first, count, v);
}

void APIENTRY Wrap_glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f) {
    AV_SCOPE("glDepthRangeIndexed");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDepthRangeIndexed(*av_t, index, n, f);
}

void APIENTRY Wrap_glDepthRangef(GLfloat n, GLfloat f) {
    AV_SCOPE("glDepthRangef");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDepthRangef(*av_t, n, f);
}

void APIENTRY Wrap_glDetachShader(GLuint program, GLuint shader) {
    AV_SCOPE("glDetachShader");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDetachShader(*av_t, program, shader);
}

void APIENTRY Wrap_glDisable(GLenum cap) {
    AV_SCOPE("glDisable");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisable(*av_t, cap);
}

void APIENTRY Wrap_glDisableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    AV_SCOPE("glDisableVertexArrayAttrib");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisableVertexArrayAttrib(*av_t, vaobj, index);
}

void APIENTRY Wrap_glDisablei(GLenum target, GLuint index) {
    AV_SCOPE("glDisablei");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisablei(*av_t, target, index);
}

void APIENTRY Wrap_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
    AV_SCOPE("glDispatchCompute");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDispatchCompute(*av_t, num_groups_x, num_groups_y, num_groups_z);
}

void APIENTRY Wrap_glDispatchComputeIndirect(GLintptr indirect) {
    AV_SCOPE("glDispatchComputeIndirect");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDispatchComputeIndirect(*av_t, indirect);
}

void APIENTRY Wrap_glDrawArraysIndirect(GLenum mode, const void* indirect) {
    AV_SCOPE("glDrawArraysIndirect");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawArraysIndirect(*av_t, mode, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indirect)));
}

void APIENTRY Wrap_glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance) {
    AV_SCOPE("glDrawArraysInstancedBaseInstance");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawArraysInstancedBaseInstance(*av_t, mode, first, count, instancecount, baseinstance);
}

void APIENTRY Wrap_glDrawBuffers(GLsizei n, const GLenum* bufs) {
    AV_SCOPE("glDrawBuffers");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawBuffers(*av_t, n, bufs);
}

void APIENTRY Wrap_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex) {
    AV_SCOPE("glDrawElementsBaseVertex");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawElementsBaseVertex(*av_t, mode, count, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices)), basevertex);
}

void APIENTRY Wrap_glDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect) {
    AV_SCOPE("glDrawElementsIndirect");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawElementsIndirect(*av_t, mode, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indirect)));
}

void APIENTRY Wrap_glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLuint baseinstance) {
    AV_SCOPE("glDrawElementsInstancedBaseInstance");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawElementsInstancedBaseInstance(*av_t, mode, count, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices)), instancecount, baseinstance);
}

void APIENTRY Wrap_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex) {
    AV_SCOPE("glDrawElementsInstancedBaseVertex");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawElementsInstancedBaseVertex(*av_t, mode, count, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices)), instancecount, basevertex);
}

void APIENTRY Wrap_glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance) {
    AV_SCOPE("glDrawElementsInstancedBaseVertexBaseInstance");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawElementsInstancedBaseVertexBaseInstance(*av_t, mode, count, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices)), instancecount, basevertex, baseinstance);
}

void APIENTRY Wrap_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices, GLint basevertex) {
    AV_SCOPE("glDrawRangeElementsBaseVertex");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawRangeElementsBaseVertex(*av_t, mode, start, end, count, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices)), basevertex);
}

void APIENTRY Wrap_glDrawTransformFeedback(GLenum mode, GLuint id) {
    AV_SCOPE("glDrawTransformFeedback");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawTransformFeedback(*av_t, mode, id);
}

void APIENTRY Wrap_glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount) {
    AV_SCOPE("glDrawTransformFeedbackInstanced");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawTransformFeedbackInstanced(*av_t, mode, id, instancecount);
}

void APIENTRY Wrap_glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream) {
    AV_SCOPE("glDrawTransformFeedbackStream");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawTransformFeedbackStream(*av_t, mode, id, stream);
}

void APIENTRY Wrap_glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount) {
    AV_SCOPE("glDrawTransformFeedbackStreamInstanced");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDrawTransformFeedbackStreamInstanced(*av_t, mode, id, stream, instancecount);
}

void APIENTRY Wrap_glEnable(GLenum cap) {
    AV_SCOPE("glEnable");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnable(*av_t, cap);
}

void APIENTRY Wrap_glEnableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    AV_SCOPE("glEnableVertexArrayAttrib");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnableVertexArrayAttrib(*av_t, vaobj, index);
}

void APIENTRY Wrap_glEnablei(GLenum target, GLuint index) {
    AV_SCOPE("glEnablei");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnablei(*av_t, target, index);
}

void APIENTRY Wrap_glEndConditionalRender(void) {
    AV_SCOPE("glEndConditionalRender");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEndConditionalRender(*av_t);
}

void APIENTRY Wrap_glEndQuery(GLenum target) {
    AV_SCOPE("glEndQuery");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEndQuery(*av_t, target);
}

void APIENTRY Wrap_glEndQueryIndexed(GLenum target, GLuint index) {
    AV_SCOPE("glEndQueryIndexed");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEndQueryIndexed(*av_t, target, index);
}

void APIENTRY Wrap_glEndTransformFeedback(void) {
    AV_SCOPE("glEndTransformFeedback");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEndTransformFeedback(*av_t);
}

GLsync APIENTRY Wrap_glFenceSync(GLenum condition, GLbitfield flags) {
    AV_SCOPE("glFenceSync");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return nullptr;
    return reinterpret_cast<GLsync>(static_cast<uintptr_t>(::av::glproto::av_glFenceSync(*av_t, condition, flags)));
}

void APIENTRY Wrap_glFramebufferParameteri(GLenum target, GLenum pname, GLint param) {
    AV_SCOPE("glFramebufferParameteri");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferParameteri(*av_t, target, pname, param);
}

void APIENTRY Wrap_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    AV_SCOPE("glFramebufferRenderbuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferRenderbuffer(*av_t, target, attachment, renderbuffertarget, renderbuffer);
}

void APIENTRY Wrap_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    AV_SCOPE("glFramebufferTexture");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferTexture(*av_t, target, attachment, texture, level);
}

void APIENTRY Wrap_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    AV_SCOPE("glFramebufferTexture1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferTexture1D(*av_t, target, attachment, textarget, texture, level);
}

void APIENTRY Wrap_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    AV_SCOPE("glFramebufferTexture2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferTexture2D(*av_t, target, attachment, textarget, texture, level);
}

void APIENTRY Wrap_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    AV_SCOPE("glFramebufferTexture3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferTexture3D(*av_t, target, attachment, textarget, texture, level, zoffset);
}

void APIENTRY Wrap_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    AV_SCOPE("glFramebufferTextureLayer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferTextureLayer(*av_t, target, attachment, texture, level, layer);
}

void APIENTRY Wrap_glFrontFace(GLenum mode) {
    AV_SCOPE("glFrontFace");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFrontFace(*av_t, mode);
}

void APIENTRY Wrap_glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    AV_SCOPE("glGenFramebuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenFramebuffers(*av_t, n, framebuffers);
}

void APIENTRY Wrap_glGenProgramPipelines(GLsizei n, GLuint* pipelines) {
    AV_SCOPE("glGenProgramPipelines");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenProgramPipelines(*av_t, n, pipelines);
}

void APIENTRY Wrap_glGenQueries(GLsizei n, GLuint* ids) {
    AV_SCOPE("glGenQueries");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenQueries(*av_t, n, ids);
}

void APIENTRY Wrap_glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    AV_SCOPE("glGenRenderbuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenRenderbuffers(*av_t, n, renderbuffers);
}

void APIENTRY Wrap_glGenSamplers(GLsizei count, GLuint* samplers) {
    AV_SCOPE("glGenSamplers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenSamplers(*av_t, count, samplers);
}

void APIENTRY Wrap_glGenTransformFeedbacks(GLsizei n, GLuint* ids) {
    AV_SCOPE("glGenTransformFeedbacks");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenTransformFeedbacks(*av_t, n, ids);
}

void APIENTRY Wrap_glGenerateMipmap(GLenum target) {
    AV_SCOPE("glGenerateMipmap");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenerateMipmap(*av_t, target);
}

void APIENTRY Wrap_glGenerateTextureMipmap(GLuint texture) {
    AV_SCOPE("glGenerateTextureMipmap");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenerateTextureMipmap(*av_t, texture);
}

void APIENTRY Wrap_glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex, GLenum pname, GLint* params) {
    AV_SCOPE("glGetActiveAtomicCounterBufferiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveAtomicCounterBufferiv(*av_t, program, bufferIndex, pname, params);
}

void APIENTRY Wrap_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
    AV_SCOPE("glGetActiveAttrib");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveAttrib(*av_t, program, index, bufSize, length, size, type, name);
}

void APIENTRY Wrap_glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name) {
    AV_SCOPE("glGetActiveSubroutineName");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveSubroutineName(*av_t, program, shadertype, index, bufSize, length, name);
}

void APIENTRY Wrap_glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name) {
    AV_SCOPE("glGetActiveSubroutineUniformName");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveSubroutineUniformName(*av_t, program, shadertype, index, bufSize, length, name);
}

void APIENTRY Wrap_glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint* values) {
    AV_SCOPE("glGetActiveSubroutineUniformiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveSubroutineUniformiv(*av_t, program, shadertype, index, pname, values);
}

void APIENTRY Wrap_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) {
    AV_SCOPE("glGetActiveUniform");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveUniform(*av_t, program, index, bufSize, length, size, type, name);
}

void APIENTRY Wrap_glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName) {
    AV_SCOPE("glGetActiveUniformBlockName");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveUniformBlockName(*av_t, program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void APIENTRY Wrap_glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params) {
    AV_SCOPE("glGetActiveUniformBlockiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveUniformBlockiv(*av_t, program, uniformBlockIndex, pname, params);
}

void APIENTRY Wrap_glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName) {
    AV_SCOPE("glGetActiveUniformName");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveUniformName(*av_t, program, uniformIndex, bufSize, length, uniformName);
}

void APIENTRY Wrap_glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params) {
    AV_SCOPE("glGetActiveUniformsiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetActiveUniformsiv(*av_t, program, uniformCount, uniformIndices, pname, params);
}

void APIENTRY Wrap_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders) {
    AV_SCOPE("glGetAttachedShaders");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetAttachedShaders(*av_t, program, maxCount, count, shaders);
}

void APIENTRY Wrap_glGetBooleani_v(GLenum target, GLuint index, GLboolean* data) {
    AV_SCOPE("glGetBooleani_v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetBooleani_v(*av_t, target, index, data);
}

void APIENTRY Wrap_glGetBooleanv(GLenum pname, GLboolean* data) {
    AV_SCOPE("glGetBooleanv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetBooleanv(*av_t, pname, data);
}

void APIENTRY Wrap_glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params) {
    AV_SCOPE("glGetBufferParameteri64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::answer_buffer_map_state(target, pname, params)) return;
    ::av::glproto::av_glGetBufferParameteri64v(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetBufferParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::answer_buffer_map_state(target, pname, params)) return;
    ::av::glproto::av_glGetBufferParameteriv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetCompressedTexImage(GLenum target, GLint level, void* img) {
    AV_SCOPE("glGetCompressedTexImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_img_slice = ::av::glicd::pack_slice_bound();
    const bool av_img_offset = !av_img_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_img_bytes = av_img_offset ? 0 : ::av::glicd::compressed_level_bytes(target, level, true);
    if (av_img_slice && !::av::glicd::slice_pack_pixels(img, av_img_bytes)) return;
    const bool av_img_staged = !av_img_offset && ::av::glicd::stage_download(img, av_img_bytes);
    if (av_img_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetCompressedTexImage(*av_t, target, level, av_img_staged ? nullptr : img, av_img_staged ? 0 : av_img_bytes, av_img_offset || av_img_staged);
    if (av_img_staged) ::av::glicd::finish_download(img, av_img_bytes);
    if (av_img_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void* pixels) {
    AV_SCOPE("glGetCompressedTextureImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : static_cast<size_t>(bufSize);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetCompressedTextureImage(*av_t, texture, level, bufSize, av_pixels_staged ? nullptr : pixels, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void* pixels) {
    AV_SCOPE("glGetCompressedTextureSubImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : static_cast<size_t>(bufSize);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetCompressedTextureSubImage(*av_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, bufSize, av_pixels_staged ? nullptr : pixels, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

GLuint APIENTRY Wrap_glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog) {
    AV_SCOPE("glGetDebugMessageLog");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetDebugMessageLog(*av_t, count, bufSize, sources, types, ids, severities, lengths, messageLog);
}

void APIENTRY Wrap_glGetDoublei_v(GLenum target, GLuint index, GLdouble* data) {
    AV_SCOPE("glGetDoublei_v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetDoublei_v(*av_t, target, index, data);
}

void APIENTRY Wrap_glGetDoublev(GLenum pname, GLdouble* data) {
    AV_SCOPE("glGetDoublev");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetDoublev(*av_t, pname, data);
}

GLenum APIENTRY Wrap_glGetError(void) {
    AV_SCOPE("glGetError");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetError(*av_t);
}

void APIENTRY Wrap_glGetFloati_v(GLenum target, GLuint index, GLfloat* data) {
    AV_SCOPE("glGetFloati_v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetFloati_v(*av_t, target, index, data);
}

void APIENTRY Wrap_glGetFloatv(GLenum pname, GLfloat* data) {
    AV_SCOPE("glGetFloatv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetFloatv(*av_t, pname, data);
}

GLint APIENTRY Wrap_glGetFragDataIndex(GLuint program, const GLchar* name) {
    AV_SCOPE("glGetFragDataIndex");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetFragDataIndex(*av_t, program, name);
}

GLint APIENTRY Wrap_glGetFragDataLocation(GLuint program, const GLchar* name) {
    AV_SCOPE("glGetFragDataLocation");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetFragDataLocation(*av_t, program, name);
}

void APIENTRY Wrap_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params) {
    AV_SCOPE("glGetFramebufferAttachmentParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetFramebufferAttachmentParameteriv(*av_t, target, attachment, pname, params);
}

void APIENTRY Wrap_glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetFramebufferParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetFramebufferParameteriv(*av_t, target, pname, params);
}

GLenum APIENTRY Wrap_glGetGraphicsResetStatus(void) {
    AV_SCOPE("glGetGraphicsResetStatus");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetGraphicsResetStatus(*av_t);
}

void APIENTRY Wrap_glGetInteger64i_v(GLenum target, GLuint index, GLint64* data) {
    AV_SCOPE("glGetInteger64i_v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetInteger64i_v(*av_t, target, index, data);
}

void APIENTRY Wrap_glGetInteger64v(GLenum pname, GLint64* data) {
    AV_SCOPE("glGetInteger64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetInteger64v(*av_t, pname, data);
}

void APIENTRY Wrap_glGetIntegeri_v(GLenum target, GLuint index, GLint* data) {
    AV_SCOPE("glGetIntegeri_v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetIntegeri_v(*av_t, target, index, data);
}

void APIENTRY Wrap_glGetIntegerv(GLenum pname, GLint* data) {
    AV_SCOPE("glGetIntegerv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetIntegerv(*av_t, pname, data);
}

void APIENTRY Wrap_glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint64* params) {
    AV_SCOPE("glGetInternalformati64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetInternalformati64v(*av_t, target, internalformat, pname, count, params);
}

void APIENTRY Wrap_glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint* params) {
    AV_SCOPE("glGetInternalformativ");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetInternalformativ(*av_t, target, internalformat, pname, count, params);
}

void APIENTRY Wrap_glGetMultisamplefv(GLenum pname, GLuint index, GLfloat* val) {
    AV_SCOPE("glGetMultisamplefv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultisamplefv(*av_t, pname, index, val);
}

void APIENTRY Wrap_glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64* params) {
    AV_SCOPE("glGetNamedBufferParameteri64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::answer_named_buffer_map_state(buffer, pname, params)) return;
    ::av::glproto::av_glGetNamedBufferParameteri64v(*av_t, buffer, pname, params);
}

void APIENTRY Wrap_glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedBufferParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::answer_named_buffer_map_state(buffer, pname, params)) return;
    ::av::glproto::av_glGetNamedBufferParameteriv(*av_t, buffer, pname, params);
}

void APIENTRY Wrap_glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedFramebufferAttachmentParameteriv");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedFramebufferAttachmentParameteriv(*av_t, framebuffer, attachment, pname, params);
}

void APIENTRY Wrap_glGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname, GLint* param) {
    AV_SCOPE("glGetNamedFramebufferParameteriv");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedFramebufferParameteriv(*av_t, framebuffer, pname, param);
}

void APIENTRY Wrap_glGetNamedRenderbufferParameteriv(GLuint renderbuffer, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedRenderbufferParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedRenderbufferParameteriv(*av_t, renderbuffer, pname, params);
}

void APIENTRY Wrap_glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label) {
    AV_SCOPE("glGetObjectLabel");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetObjectLabel(*av_t, identifier, name, bufSize, length, label);
}

void APIENTRY Wrap_glGetObjectPtrLabel(GLsync ptr, GLsizei bufSize, GLsizei* length, GLchar* label) {
    AV_SCOPE("glGetObjectPtrLabel");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetObjectPtrLabel(*av_t, reinterpret_cast<uint64_t>(ptr), bufSize, length, label);
}

void APIENTRY Wrap_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary) {
    AV_SCOPE("glGetProgramBinary");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramBinary(*av_t, program, bufSize, length, binaryFormat, binary);
}

void APIENTRY Wrap_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    AV_SCOPE("glGetProgramInfoLog");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramInfoLog(*av_t, program, bufSize, length, infoLog);
}

void APIENTRY Wrap_glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint* params) {
    AV_SCOPE("glGetProgramInterfaceiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramInterfaceiv(*av_t, program, programInterface, pname, params);
}

void APIENTRY Wrap_glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    AV_SCOPE("glGetProgramPipelineInfoLog");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramPipelineInfoLog(*av_t, pipeline, bufSize, length, infoLog);
}

void APIENTRY Wrap_glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint* params) {
    AV_SCOPE("glGetProgramPipelineiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramPipelineiv(*av_t, pipeline, pname, params);
}

GLuint APIENTRY Wrap_glGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar* name) {
    AV_SCOPE("glGetProgramResourceIndex");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetProgramResourceIndex(*av_t, program, programInterface, name);
}

GLint APIENTRY Wrap_glGetProgramResourceLocation(GLuint program, GLenum programInterface, const GLchar* name) {
    AV_SCOPE("glGetProgramResourceLocation");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetProgramResourceLocation(*av_t, program, programInterface, name);
}

GLint APIENTRY Wrap_glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface, const GLchar* name) {
    AV_SCOPE("glGetProgramResourceLocationIndex");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetProgramResourceLocationIndex(*av_t, program, programInterface, name);
}

void APIENTRY Wrap_glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name) {
    AV_SCOPE("glGetProgramResourceName");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramResourceName(*av_t, program, programInterface, index, bufSize, length, name);
}

void APIENTRY Wrap_glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum* props, GLsizei count, GLsizei* length, GLint* params) {
    AV_SCOPE("glGetProgramResourceiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramResourceiv(*av_t, program, programInterface, index, propCount, props, count, length, params);
}

void APIENTRY Wrap_glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint* values) {
    AV_SCOPE("glGetProgramStageiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramStageiv(*av_t, program, shadertype, pname, values);
}

void APIENTRY Wrap_glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
    AV_SCOPE("glGetProgramiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetProgramiv(*av_t, program, pname, params);
}

void APIENTRY Wrap_glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    AV_SCOPE("glGetQueryBufferObjecti64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glGetQueryBufferObjecti64v(*av_t, id, buffer, pname, offset);
}

void APIENTRY Wrap_glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    AV_SCOPE("glGetQueryBufferObjectiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glGetQueryBufferObjectiv(*av_t, id, buffer, pname, offset);
}

void APIENTRY Wrap_glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    AV_SCOPE("glGetQueryBufferObjectui64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glGetQueryBufferObjectui64v(*av_t, id, buffer, pname, offset);
}

void APIENTRY Wrap_glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    AV_SCOPE("glGetQueryBufferObjectuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glGetQueryBufferObjectuiv(*av_t, id, buffer, pname, offset);
}

void APIENTRY Wrap_glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint* params) {
    AV_SCOPE("glGetQueryIndexediv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetQueryIndexediv(*av_t, target, index, pname, params);
}

void APIENTRY Wrap_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params) {
    AV_SCOPE("glGetQueryObjecti64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_params_offset = ::av::glicd::query_buffer_bound();
    const size_t av_params_bytes = av_params_offset ? 0 : ::av::glproto::pname_count(*av_t, pname) * sizeof(GLint64);
    if (av_params_offset) ::av::glicd::host_writes_target(0x9192u);
    ::av::glproto::av_glGetQueryObjecti64v(*av_t, id, pname, params, av_params_bytes, av_params_offset);
}

void APIENTRY Wrap_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    AV_SCOPE("glGetQueryObjectiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_params_offset = ::av::glicd::query_buffer_bound();
    const size_t av_params_bytes = av_params_offset ? 0 : ::av::glproto::pname_count(*av_t, pname) * sizeof(GLint);
    if (av_params_offset) ::av::glicd::host_writes_target(0x9192u);
    ::av::glproto::av_glGetQueryObjectiv(*av_t, id, pname, params, av_params_bytes, av_params_offset);
}

void APIENTRY Wrap_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) {
    AV_SCOPE("glGetQueryObjectui64v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_params_offset = ::av::glicd::query_buffer_bound();
    const size_t av_params_bytes = av_params_offset ? 0 : ::av::glproto::pname_count(*av_t, pname) * sizeof(GLuint64);
    if (av_params_offset) ::av::glicd::host_writes_target(0x9192u);
    ::av::glproto::av_glGetQueryObjectui64v(*av_t, id, pname, params, av_params_bytes, av_params_offset);
}

void APIENTRY Wrap_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
    AV_SCOPE("glGetQueryObjectuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_params_offset = ::av::glicd::query_buffer_bound();
    const size_t av_params_bytes = av_params_offset ? 0 : ::av::glproto::pname_count(*av_t, pname) * sizeof(GLuint);
    if (av_params_offset) ::av::glicd::host_writes_target(0x9192u);
    ::av::glproto::av_glGetQueryObjectuiv(*av_t, id, pname, params, av_params_bytes, av_params_offset);
}

void APIENTRY Wrap_glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetQueryiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetQueryiv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetRenderbufferParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetRenderbufferParameteriv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint* params) {
    AV_SCOPE("glGetSamplerParameterIiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetSamplerParameterIiv(*av_t, sampler, pname, params);
}

void APIENTRY Wrap_glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint* params) {
    AV_SCOPE("glGetSamplerParameterIuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetSamplerParameterIuiv(*av_t, sampler, pname, params);
}

void APIENTRY Wrap_glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetSamplerParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetSamplerParameterfv(*av_t, sampler, pname, params);
}

void APIENTRY Wrap_glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint* params) {
    AV_SCOPE("glGetSamplerParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetSamplerParameteriv(*av_t, sampler, pname, params);
}

void APIENTRY Wrap_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    AV_SCOPE("glGetShaderInfoLog");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetShaderInfoLog(*av_t, shader, bufSize, length, infoLog);
}

void APIENTRY Wrap_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
    AV_SCOPE("glGetShaderPrecisionFormat");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetShaderPrecisionFormat(*av_t, shadertype, precisiontype, range, precision);
}

void APIENTRY Wrap_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source) {
    AV_SCOPE("glGetShaderSource");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetShaderSource(*av_t, shader, bufSize, length, source);
}

void APIENTRY Wrap_glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    AV_SCOPE("glGetShaderiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetShaderiv(*av_t, shader, pname, params);
}

GLuint APIENTRY Wrap_glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar* name) {
    AV_SCOPE("glGetSubroutineIndex");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetSubroutineIndex(*av_t, program, shadertype, name);
}

GLint APIENTRY Wrap_glGetSubroutineUniformLocation(GLuint program, GLenum shadertype, const GLchar* name) {
    AV_SCOPE("glGetSubroutineUniformLocation");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetSubroutineUniformLocation(*av_t, program, shadertype, name);
}

void APIENTRY Wrap_glGetSynciv(GLsync sync, GLenum pname, GLsizei count, GLsizei* length, GLint* values) {
    AV_SCOPE("glGetSynciv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetSynciv(*av_t, reinterpret_cast<uint64_t>(sync), pname, count, length, values);
}

void APIENTRY Wrap_glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    AV_SCOPE("glGetTexImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::level_bytes(target, level, format, type);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetTexImage(*av_t, target, level, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTexLevelParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexLevelParameterfv(*av_t, target, level, pname, params);
}

void APIENTRY Wrap_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTexLevelParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexLevelParameteriv(*av_t, target, level, pname, params);
}

void APIENTRY Wrap_glGetTexParameterIiv(GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTexParameterIiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexParameterIiv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint* params) {
    AV_SCOPE("glGetTexParameterIuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexParameterIuiv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTexParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexParameterfv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTexParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexParameteriv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
    AV_SCOPE("glGetTextureImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : static_cast<size_t>(bufSize);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetTextureImage(*av_t, texture, level, format, type, bufSize, av_pixels_staged ? nullptr : pixels, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTextureLevelParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureLevelParameterfv(*av_t, texture, level, pname, params);
}

void APIENTRY Wrap_glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTextureLevelParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureLevelParameteriv(*av_t, texture, level, pname, params);
}

void APIENTRY Wrap_glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTextureParameterIiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameterIiv(*av_t, texture, pname, params);
}

void APIENTRY Wrap_glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint* params) {
    AV_SCOPE("glGetTextureParameterIuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameterIuiv(*av_t, texture, pname, params);
}

void APIENTRY Wrap_glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTextureParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameterfv(*av_t, texture, pname, params);
}

void APIENTRY Wrap_glGetTextureParameteriv(GLuint texture, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTextureParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameteriv(*av_t, texture, pname, params);
}

void APIENTRY Wrap_glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
    AV_SCOPE("glGetTextureSubImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : static_cast<size_t>(bufSize);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetTextureSubImage(*av_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, bufSize, av_pixels_staged ? nullptr : pixels, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name) {
    AV_SCOPE("glGetTransformFeedbackVarying");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTransformFeedbackVarying(*av_t, program, index, bufSize, length, size, type, name);
}

void APIENTRY Wrap_glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64* param) {
    AV_SCOPE("glGetTransformFeedbacki64_v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTransformFeedbacki64_v(*av_t, xfb, pname, index, param);
}

void APIENTRY Wrap_glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint* param) {
    AV_SCOPE("glGetTransformFeedbacki_v");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTransformFeedbacki_v(*av_t, xfb, pname, index, param);
}

void APIENTRY Wrap_glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint* param) {
    AV_SCOPE("glGetTransformFeedbackiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTransformFeedbackiv(*av_t, xfb, pname, param);
}

GLuint APIENTRY Wrap_glGetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName) {
    AV_SCOPE("glGetUniformBlockIndex");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetUniformBlockIndex(*av_t, program, uniformBlockName);
}

void APIENTRY Wrap_glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar*const* uniformNames, GLuint* uniformIndices) {
    AV_SCOPE("glGetUniformIndices");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformIndices(*av_t, program, uniformCount, uniformNames, uniformIndices);
}

void APIENTRY Wrap_glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint* params) {
    AV_SCOPE("glGetUniformSubroutineuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformSubroutineuiv(*av_t, shadertype, location, params);
}

void APIENTRY Wrap_glGetUniformdv(GLuint program, GLint location, GLdouble* params) {
    AV_SCOPE("glGetUniformdv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformdv(*av_t, program, location, params);
}

void APIENTRY Wrap_glGetUniformfv(GLuint program, GLint location, GLfloat* params) {
    AV_SCOPE("glGetUniformfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformfv(*av_t, program, location, params);
}

void APIENTRY Wrap_glGetUniformiv(GLuint program, GLint location, GLint* params) {
    AV_SCOPE("glGetUniformiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformiv(*av_t, program, location, params);
}

void APIENTRY Wrap_glGetUniformuiv(GLuint program, GLint location, GLuint* params) {
    AV_SCOPE("glGetUniformuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformuiv(*av_t, program, location, params);
}

void APIENTRY Wrap_glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname, GLint64* param) {
    AV_SCOPE("glGetVertexArrayIndexed64iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexArrayIndexed64iv(*av_t, vaobj, index, pname, param);
}

void APIENTRY Wrap_glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint* param) {
    AV_SCOPE("glGetVertexArrayIndexediv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexArrayIndexediv(*av_t, vaobj, index, pname, param);
}

void APIENTRY Wrap_glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint* param) {
    AV_SCOPE("glGetVertexArrayiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexArrayiv(*av_t, vaobj, pname, param);
}

void APIENTRY Wrap_glGetVertexAttribIiv(GLuint index, GLenum pname, GLint* params) {
    AV_SCOPE("glGetVertexAttribIiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexAttribIiv(*av_t, index, pname, params);
}

void APIENTRY Wrap_glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params) {
    AV_SCOPE("glGetVertexAttribIuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexAttribIuiv(*av_t, index, pname, params);
}

void APIENTRY Wrap_glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble* params) {
    AV_SCOPE("glGetVertexAttribLdv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexAttribLdv(*av_t, index, pname, params);
}

void APIENTRY Wrap_glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble* params) {
    AV_SCOPE("glGetVertexAttribdv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexAttribdv(*av_t, index, pname, params);
}

void APIENTRY Wrap_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetVertexAttribfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexAttribfv(*av_t, index, pname, params);
}

void APIENTRY Wrap_glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
    AV_SCOPE("glGetVertexAttribiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexAttribiv(*av_t, index, pname, params);
}

void APIENTRY Wrap_glGetnCompressedTexImage(GLenum target, GLint lod, GLsizei bufSize, void* pixels) {
    AV_SCOPE("glGetnCompressedTexImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : static_cast<size_t>(bufSize);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetnCompressedTexImage(*av_t, target, lod, bufSize, av_pixels_staged ? nullptr : pixels, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
    AV_SCOPE("glGetnTexImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : static_cast<size_t>(bufSize);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetnTexImage(*av_t, target, level, format, type, bufSize, av_pixels_staged ? nullptr : pixels, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble* params) {
    AV_SCOPE("glGetnUniformdv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetnUniformdv(*av_t, program, location, bufSize, params);
}

void APIENTRY Wrap_glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat* params) {
    AV_SCOPE("glGetnUniformfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetnUniformfv(*av_t, program, location, bufSize, params);
}

void APIENTRY Wrap_glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint* params) {
    AV_SCOPE("glGetnUniformiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetnUniformiv(*av_t, program, location, bufSize, params);
}

void APIENTRY Wrap_glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint* params) {
    AV_SCOPE("glGetnUniformuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetnUniformuiv(*av_t, program, location, bufSize, params);
}

void APIENTRY Wrap_glHint(GLenum target, GLenum mode) {
    AV_SCOPE("glHint");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glHint(*av_t, target, mode);
}

void APIENTRY Wrap_glInvalidateBufferData(GLuint buffer) {
    AV_SCOPE("glInvalidateBufferData");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::invalidate_named_buffer(buffer)) return;
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glInvalidateBufferData(*av_t, buffer);
}

void APIENTRY Wrap_glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    AV_SCOPE("glInvalidateBufferSubData");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::invalidate_named_buffer_range(buffer, offset, length)) return;
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glInvalidateBufferSubData(*av_t, buffer, offset, length);
}

void APIENTRY Wrap_glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments) {
    AV_SCOPE("glInvalidateFramebuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glInvalidateFramebuffer(*av_t, target, numAttachments, attachments);
}

void APIENTRY Wrap_glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments) {
    AV_SCOPE("glInvalidateNamedFramebufferData");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glInvalidateNamedFramebufferData(*av_t, framebuffer, numAttachments, attachments);
}

void APIENTRY Wrap_glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glInvalidateNamedFramebufferSubData");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glInvalidateNamedFramebufferSubData(*av_t, framebuffer, numAttachments, attachments, x, y, width, height);
}

void APIENTRY Wrap_glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glInvalidateSubFramebuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glInvalidateSubFramebuffer(*av_t, target, numAttachments, attachments, x, y, width, height);
}

void APIENTRY Wrap_glInvalidateTexImage(GLuint texture, GLint level) {
    AV_SCOPE("glInvalidateTexImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glInvalidateTexImage(*av_t, texture, level);
}

void APIENTRY Wrap_glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth) {
    AV_SCOPE("glInvalidateTexSubImage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glInvalidateTexSubImage(*av_t, texture, level, xoffset, yoffset, zoffset, width, height, depth);
}

GLboolean APIENTRY Wrap_glIsBuffer(GLuint buffer) {
    AV_SCOPE("glIsBuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsBuffer(*av_t, buffer);
}

GLboolean APIENTRY Wrap_glIsEnabled(GLenum cap) {
    AV_SCOPE("glIsEnabled");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsEnabled(*av_t, cap);
}

GLboolean APIENTRY Wrap_glIsEnabledi(GLenum target, GLuint index) {
    AV_SCOPE("glIsEnabledi");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsEnabledi(*av_t, target, index);
}

GLboolean APIENTRY Wrap_glIsFramebuffer(GLuint framebuffer) {
    AV_SCOPE("glIsFramebuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsFramebuffer(*av_t, framebuffer);
}

GLboolean APIENTRY Wrap_glIsProgram(GLuint program) {
    AV_SCOPE("glIsProgram");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsProgram(*av_t, program);
}

GLboolean APIENTRY Wrap_glIsProgramPipeline(GLuint pipeline) {
    AV_SCOPE("glIsProgramPipeline");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsProgramPipeline(*av_t, pipeline);
}

GLboolean APIENTRY Wrap_glIsQuery(GLuint id) {
    AV_SCOPE("glIsQuery");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsQuery(*av_t, id);
}

GLboolean APIENTRY Wrap_glIsRenderbuffer(GLuint renderbuffer) {
    AV_SCOPE("glIsRenderbuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsRenderbuffer(*av_t, renderbuffer);
}

GLboolean APIENTRY Wrap_glIsSampler(GLuint sampler) {
    AV_SCOPE("glIsSampler");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsSampler(*av_t, sampler);
}

GLboolean APIENTRY Wrap_glIsShader(GLuint shader) {
    AV_SCOPE("glIsShader");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsShader(*av_t, shader);
}

GLboolean APIENTRY Wrap_glIsSync(GLsync sync) {
    AV_SCOPE("glIsSync");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsSync(*av_t, reinterpret_cast<uint64_t>(sync));
}

GLboolean APIENTRY Wrap_glIsTexture(GLuint texture) {
    AV_SCOPE("glIsTexture");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsTexture(*av_t, texture);
}

GLboolean APIENTRY Wrap_glIsTransformFeedback(GLuint id) {
    AV_SCOPE("glIsTransformFeedback");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsTransformFeedback(*av_t, id);
}

GLboolean APIENTRY Wrap_glIsVertexArray(GLuint array) {
    AV_SCOPE("glIsVertexArray");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsVertexArray(*av_t, array);
}

void APIENTRY Wrap_glLineWidth(GLfloat width) {
    AV_SCOPE("glLineWidth");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLineWidth(*av_t, width);
}

void APIENTRY Wrap_glLogicOp(GLenum opcode) {
    AV_SCOPE("glLogicOp");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLogicOp(*av_t, opcode);
}

void APIENTRY Wrap_glMemoryBarrier(GLbitfield barriers) {
    AV_SCOPE("glMemoryBarrier");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMemoryBarrier(*av_t, barriers);
}

void APIENTRY Wrap_glMemoryBarrierByRegion(GLbitfield barriers) {
    AV_SCOPE("glMemoryBarrierByRegion");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMemoryBarrierByRegion(*av_t, barriers);
}

void APIENTRY Wrap_glMinSampleShading(GLfloat value) {
    AV_SCOPE("glMinSampleShading");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMinSampleShading(*av_t, value);
}

void APIENTRY Wrap_glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    AV_SCOPE("glMultiDrawArrays");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiDrawArrays(*av_t, mode, first, count, drawcount);
}

void APIENTRY Wrap_glMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride) {
    AV_SCOPE("glMultiDrawArraysIndirect");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiDrawArraysIndirect(*av_t, mode, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indirect)), drawcount, stride);
}

void APIENTRY Wrap_glMultiDrawArraysIndirectCount(GLenum mode, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    AV_SCOPE("glMultiDrawArraysIndirectCount");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiDrawArraysIndirectCount(*av_t, mode, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indirect)), drawcount, maxdrawcount, stride);
}

void APIENTRY Wrap_glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount) {
    AV_SCOPE("glMultiDrawElements");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiDrawElements(*av_t, mode, count, type, ::av::glicd::offsets_of(indices, static_cast<size_t>(drawcount)), drawcount);
}

void APIENTRY Wrap_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount, const GLint* basevertex) {
    AV_SCOPE("glMultiDrawElementsBaseVertex");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiDrawElementsBaseVertex(*av_t, mode, count, type, ::av::glicd::offsets_of(indices, static_cast<size_t>(drawcount)), drawcount, basevertex);
}

void APIENTRY Wrap_glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride) {
    AV_SCOPE("glMultiDrawElementsIndirect");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiDrawElementsIndirect(*av_t, mode, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indirect)), drawcount, stride);
}

void APIENTRY Wrap_glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    AV_SCOPE("glMultiDrawElementsIndirectCount");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiDrawElementsIndirectCount(*av_t, mode, type, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indirect)), drawcount, maxdrawcount, stride);
}

void APIENTRY Wrap_glMultiTexCoordP1ui(GLenum texture, GLenum type, GLuint coords) {
    AV_SCOPE("glMultiTexCoordP1ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP1ui(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glMultiTexCoordP1uiv(GLenum texture, GLenum type, const GLuint* coords) {
    AV_SCOPE("glMultiTexCoordP1uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP1uiv(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glMultiTexCoordP2ui(GLenum texture, GLenum type, GLuint coords) {
    AV_SCOPE("glMultiTexCoordP2ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP2ui(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glMultiTexCoordP2uiv(GLenum texture, GLenum type, const GLuint* coords) {
    AV_SCOPE("glMultiTexCoordP2uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP2uiv(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glMultiTexCoordP3ui(GLenum texture, GLenum type, GLuint coords) {
    AV_SCOPE("glMultiTexCoordP3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP3ui(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glMultiTexCoordP3uiv(GLenum texture, GLenum type, const GLuint* coords) {
    AV_SCOPE("glMultiTexCoordP3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP3uiv(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glMultiTexCoordP4ui(GLenum texture, GLenum type, GLuint coords) {
    AV_SCOPE("glMultiTexCoordP4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP4ui(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glMultiTexCoordP4uiv(GLenum texture, GLenum type, const GLuint* coords) {
    AV_SCOPE("glMultiTexCoordP4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordP4uiv(*av_t, texture, type, coords);
}

void APIENTRY Wrap_glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum buf) {
    AV_SCOPE("glNamedFramebufferDrawBuffer");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferDrawBuffer(*av_t, framebuffer, buf);
}

void APIENTRY Wrap_glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
    AV_SCOPE("glNamedFramebufferDrawBuffers");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferDrawBuffers(*av_t, framebuffer, n, bufs);
}

void APIENTRY Wrap_glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param) {
    AV_SCOPE("glNamedFramebufferParameteri");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferParameteri(*av_t, framebuffer, pname, param);
}

void APIENTRY Wrap_glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum src) {
    AV_SCOPE("glNamedFramebufferReadBuffer");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferReadBuffer(*av_t, framebuffer, src);
}

void APIENTRY Wrap_glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    AV_SCOPE("glNamedFramebufferRenderbuffer");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferRenderbuffer(*av_t, framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void APIENTRY Wrap_glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    AV_SCOPE("glNamedFramebufferTexture");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTexture(*av_t, framebuffer, attachment, texture, level);
}

void APIENTRY Wrap_glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    AV_SCOPE("glNamedFramebufferTextureLayer");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTextureLayer(*av_t, framebuffer, attachment, texture, level, layer);
}

void APIENTRY Wrap_glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glNamedRenderbufferStorage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedRenderbufferStorage(*av_t, renderbuffer, internalformat, width, height);
}

void APIENTRY Wrap_glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glNamedRenderbufferStorageMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedRenderbufferStorageMultisample(*av_t, renderbuffer, samples, internalformat, width, height);
}

void APIENTRY Wrap_glNormalP3ui(GLenum type, GLuint coords) {
    AV_SCOPE("glNormalP3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormalP3ui(*av_t, type, coords);
}

void APIENTRY Wrap_glNormalP3uiv(GLenum type, const GLuint* coords) {
    AV_SCOPE("glNormalP3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormalP3uiv(*av_t, type, coords);
}

void APIENTRY Wrap_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label) {
    AV_SCOPE("glObjectLabel");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glObjectLabel(*av_t, identifier, name, length, label);
}

void APIENTRY Wrap_glObjectPtrLabel(GLsync ptr, GLsizei length, const GLchar* label) {
    AV_SCOPE("glObjectPtrLabel");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glObjectPtrLabel(*av_t, reinterpret_cast<uint64_t>(ptr), length, label);
}

void APIENTRY Wrap_glPatchParameterfv(GLenum pname, const GLfloat* values) {
    AV_SCOPE("glPatchParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPatchParameterfv(*av_t, pname, values);
}

void APIENTRY Wrap_glPatchParameteri(GLenum pname, GLint value) {
    AV_SCOPE("glPatchParameteri");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPatchParameteri(*av_t, pname, value);
}

void APIENTRY Wrap_glPauseTransformFeedback(void) {
    AV_SCOPE("glPauseTransformFeedback");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPauseTransformFeedback(*av_t);
}

void APIENTRY Wrap_glPointParameterf(GLenum pname, GLfloat param) {
    AV_SCOPE("glPointParameterf");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPointParameterf(*av_t, pname, param);
}

void APIENTRY Wrap_glPointParameterfv(GLenum pname, const GLfloat* params) {
    AV_SCOPE("glPointParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPointParameterfv(*av_t, pname, params);
}

void APIENTRY Wrap_glPointParameteri(GLenum pname, GLint param) {
    AV_SCOPE("glPointParameteri");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPointParameteri(*av_t, pname, param);
}

void APIENTRY Wrap_glPointParameteriv(GLenum pname, const GLint* params) {
    AV_SCOPE("glPointParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPointParameteriv(*av_t, pname, params);
}

void APIENTRY Wrap_glPointSize(GLfloat size) {
    AV_SCOPE("glPointSize");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPointSize(*av_t, size);
}

void APIENTRY Wrap_glPolygonMode(GLenum face, GLenum mode) {
    AV_SCOPE("glPolygonMode");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPolygonMode(*av_t, face, mode);
}

void APIENTRY Wrap_glPolygonOffset(GLfloat factor, GLfloat units) {
    AV_SCOPE("glPolygonOffset");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPolygonOffset(*av_t, factor, units);
}

void APIENTRY Wrap_glPolygonOffsetClamp(GLfloat factor, GLfloat units, GLfloat clamp) {
    AV_SCOPE("glPolygonOffsetClamp");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPolygonOffsetClamp(*av_t, factor, units, clamp);
}

void APIENTRY Wrap_glPopDebugGroup(void) {
    AV_SCOPE("glPopDebugGroup");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPopDebugGroup(*av_t);
}

void APIENTRY Wrap_glPrimitiveRestartIndex(GLuint index) {
    AV_SCOPE("glPrimitiveRestartIndex");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPrimitiveRestartIndex(*av_t, index);
}

void APIENTRY Wrap_glProgramBinary(GLuint program, GLenum binaryFormat, const void* binary, GLsizei length) {
    AV_SCOPE("glProgramBinary");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramBinary(*av_t, program, binaryFormat, binary, length);
}

void APIENTRY Wrap_glProgramParameteri(GLuint program, GLenum pname, GLint value) {
    AV_SCOPE("glProgramParameteri");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramParameteri(*av_t, program, pname, value);
}

void APIENTRY Wrap_glProgramUniform1d(GLuint program, GLint location, GLdouble v0) {
    AV_SCOPE("glProgramUniform1d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1d(*av_t, program, location, v0);
}

void APIENTRY Wrap_glProgramUniform1dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform1dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1dv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform1f(GLuint program, GLint location, GLfloat v0) {
    AV_SCOPE("glProgramUniform1f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1f(*av_t, program, location, v0);
}

void APIENTRY Wrap_glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform1fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1fv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform1i(GLuint program, GLint location, GLint v0) {
    AV_SCOPE("glProgramUniform1i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1i(*av_t, program, location, v0);
}

void APIENTRY Wrap_glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform1iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1iv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform1ui(GLuint program, GLint location, GLuint v0) {
    AV_SCOPE("glProgramUniform1ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1ui(*av_t, program, location, v0);
}

void APIENTRY Wrap_glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform1uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1uiv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2d(GLuint program, GLint location, GLdouble v0, GLdouble v1) {
    AV_SCOPE("glProgramUniform2d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2d(*av_t, program, location, v0, v1);
}

void APIENTRY Wrap_glProgramUniform2dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2dv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    AV_SCOPE("glProgramUniform2f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2f(*av_t, program, location, v0, v1);
}

void APIENTRY Wrap_glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2fv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1) {
    AV_SCOPE("glProgramUniform2i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2i(*av_t, program, location, v0, v1);
}

void APIENTRY Wrap_glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform2iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2iv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1) {
    AV_SCOPE("glProgramUniform2ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2ui(*av_t, program, location, v0, v1);
}

void APIENTRY Wrap_glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform2uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2uiv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2) {
    AV_SCOPE("glProgramUniform3d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3d(*av_t, program, location, v0, v1, v2);
}

void APIENTRY Wrap_glProgramUniform3dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3dv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    AV_SCOPE("glProgramUniform3f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3f(*av_t, program, location, v0, v1, v2);
}

void APIENTRY Wrap_glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3fv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    AV_SCOPE("glProgramUniform3i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3i(*av_t, program, location, v0, v1, v2);
}

void APIENTRY Wrap_glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform3iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3iv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    AV_SCOPE("glProgramUniform3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3ui(*av_t, program, location, v0, v1, v2);
}

void APIENTRY Wrap_glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3uiv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3) {
    AV_SCOPE("glProgramUniform4d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4d(*av_t, program, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glProgramUniform4dv(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4dv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    AV_SCOPE("glProgramUniform4f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4f(*av_t, program, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4fv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    AV_SCOPE("glProgramUniform4i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4i(*av_t, program, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform4iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4iv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    AV_SCOPE("glProgramUniform4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4ui(*av_t, program, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4uiv(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix2x3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x3dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix2x3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x3fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix2x4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x4dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix2x4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x4fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix3x2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x2dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix3x2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x2fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix3x4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x4dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix3x4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x4fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix4x2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x2dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix4x2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x2fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix4x3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x3dv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix4x3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x3fv(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProvokingVertex(GLenum mode) {
    AV_SCOPE("glProvokingVertex");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProvokingVertex(*av_t, mode);
}

void APIENTRY Wrap_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message) {
    AV_SCOPE("glPushDebugGroup");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPushDebugGroup(*av_t, source, id, length, message);
}

void APIENTRY Wrap_glQueryCounter(GLuint id, GLenum target) {
    AV_SCOPE("glQueryCounter");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glQueryCounter(*av_t, id, target);
}

void APIENTRY Wrap_glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void* data) {
    AV_SCOPE("glReadnPixels");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::pack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_data_bytes = av_data_offset ? 0 : static_cast<size_t>(bufSize);
    if (av_data_slice && !::av::glicd::slice_pack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_download(data, av_data_bytes);
    if (av_data_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glReadnPixels(*av_t, x, y, width, height, format, type, bufSize, av_data_staged ? nullptr : data, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_download(data, av_data_bytes);
    if (av_data_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glReleaseShaderCompiler(void) {
    AV_SCOPE("glReleaseShaderCompiler");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glReleaseShaderCompiler(*av_t);
}

void APIENTRY Wrap_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glRenderbufferStorage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRenderbufferStorage(*av_t, target, internalformat, width, height);
}

void APIENTRY Wrap_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glRenderbufferStorageMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRenderbufferStorageMultisample(*av_t, target, samples, internalformat, width, height);
}

void APIENTRY Wrap_glResumeTransformFeedback(void) {
    AV_SCOPE("glResumeTransformFeedback");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glResumeTransformFeedback(*av_t);
}

void APIENTRY Wrap_glSampleCoverage(GLfloat value, GLboolean invert) {
    AV_SCOPE("glSampleCoverage");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSampleCoverage(*av_t, value, invert);
}

void APIENTRY Wrap_glSampleMaski(GLuint maskNumber, GLbitfield mask) {
    AV_SCOPE("glSampleMaski");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSampleMaski(*av_t, maskNumber, mask);
}

void APIENTRY Wrap_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint* param) {
    AV_SCOPE("glSamplerParameterIiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSamplerParameterIiv(*av_t, sampler, pname, param);
}

void APIENTRY Wrap_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint* param) {
    AV_SCOPE("glSamplerParameterIuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSamplerParameterIuiv(*av_t, sampler, pname, param);
}

void APIENTRY Wrap_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    AV_SCOPE("glSamplerParameterf");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSamplerParameterf(*av_t, sampler, pname, param);
}

void APIENTRY Wrap_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* param) {
    AV_SCOPE("glSamplerParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSamplerParameterfv(*av_t, sampler, pname, param);
}

void APIENTRY Wrap_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    AV_SCOPE("glSamplerParameteri");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSamplerParameteri(*av_t, sampler, pname, param);
}

void APIENTRY Wrap_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint* param) {
    AV_SCOPE("glSamplerParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSamplerParameteriv(*av_t, sampler, pname, param);
}

void APIENTRY Wrap_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glScissor");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScissor(*av_t, x, y, width, height);
}

void APIENTRY Wrap_glScissorArrayv(GLuint first, GLsizei count, const GLint* v) {
    AV_SCOPE("glScissorArrayv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScissorArrayv(*av_t, first, count, v);
}

void APIENTRY Wrap_glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height) {
    AV_SCOPE("glScissorIndexed");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScissorIndexed(*av_t, index, left, bottom, width, height);
}

void APIENTRY Wrap_glScissorIndexedv(GLuint index, const GLint* v) {
    AV_SCOPE("glScissorIndexedv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScissorIndexedv(*av_t, index, v);
}

void APIENTRY Wrap_glSecondaryColorP3ui(GLenum type, GLuint color) {
    AV_SCOPE("glSecondaryColorP3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColorP3ui(*av_t, type, color);
}

void APIENTRY Wrap_glSecondaryColorP3uiv(GLenum type, const GLuint* color) {
    AV_SCOPE("glSecondaryColorP3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColorP3uiv(*av_t, type, color);
}

void APIENTRY Wrap_glShaderBinary(GLsizei count, const GLuint* shaders, GLenum binaryFormat, const void* binary, GLsizei length) {
    AV_SCOPE("glShaderBinary");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glShaderBinary(*av_t, count, shaders, binaryFormat, binary, length);
}

void APIENTRY Wrap_glShaderSource(GLuint shader, GLsizei count, const GLchar*const* string, const GLint* length) {
    AV_SCOPE("glShaderSource");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glShaderSource(*av_t, shader, count, string, length);
}

void APIENTRY Wrap_glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding) {
    AV_SCOPE("glShaderStorageBlockBinding");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glShaderStorageBlockBinding(*av_t, program, storageBlockIndex, storageBlockBinding);
}

void APIENTRY Wrap_glSpecializeShader(GLuint shader, const GLchar* pEntryPoint, GLuint numSpecializationConstants, const GLuint* pConstantIndex, const GLuint* pConstantValue) {
    AV_SCOPE("glSpecializeShader");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSpecializeShader(*av_t, shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

void APIENTRY Wrap_glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    AV_SCOPE("glStencilFunc");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glStencilFunc(*av_t, func, ref, mask);
}

void APIENTRY Wrap_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    AV_SCOPE("glStencilFuncSeparate");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glStencilFuncSeparate(*av_t, face, func, ref, mask);
}

void APIENTRY Wrap_glStencilMask(GLuint mask) {
    AV_SCOPE("glStencilMask");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glStencilMask(*av_t, mask);
}

void APIENTRY Wrap_glStencilMaskSeparate(GLenum face, GLuint mask) {
    AV_SCOPE("glStencilMaskSeparate");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glStencilMaskSeparate(*av_t, face, mask);
}

void APIENTRY Wrap_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    AV_SCOPE("glStencilOp");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glStencilOp(*av_t, fail, zfail, zpass);
}

void APIENTRY Wrap_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    AV_SCOPE("glStencilOpSeparate");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glStencilOpSeparate(*av_t, face, sfail, dpfail, dppass);
}

void APIENTRY Wrap_glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    AV_SCOPE("glTexBuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexBuffer(*av_t, target, internalformat, buffer);
}

void APIENTRY Wrap_glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    AV_SCOPE("glTexBufferRange");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexBufferRange(*av_t, target, internalformat, buffer, offset, size);
}

void APIENTRY Wrap_glTexCoordP1ui(GLenum type, GLuint coords) {
    AV_SCOPE("glTexCoordP1ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP1ui(*av_t, type, coords);
}

void APIENTRY Wrap_glTexCoordP1uiv(GLenum type, const GLuint* coords) {
    AV_SCOPE("glTexCoordP1uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP1uiv(*av_t, type, coords);
}

void APIENTRY Wrap_glTexCoordP2ui(GLenum type, GLuint coords) {
    AV_SCOPE("glTexCoordP2ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP2ui(*av_t, type, coords);
}

void APIENTRY Wrap_glTexCoordP2uiv(GLenum type, const GLuint* coords) {
    AV_SCOPE("glTexCoordP2uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP2uiv(*av_t, type, coords);
}

void APIENTRY Wrap_glTexCoordP3ui(GLenum type, GLuint coords) {
    AV_SCOPE("glTexCoordP3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP3ui(*av_t, type, coords);
}

void APIENTRY Wrap_glTexCoordP3uiv(GLenum type, const GLuint* coords) {
    AV_SCOPE("glTexCoordP3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP3uiv(*av_t, type, coords);
}

void APIENTRY Wrap_glTexCoordP4ui(GLenum type, GLuint coords) {
    AV_SCOPE("glTexCoordP4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP4ui(*av_t, type, coords);
}

void APIENTRY Wrap_glTexCoordP4uiv(GLenum type, const GLuint* coords) {
    AV_SCOPE("glTexCoordP4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordP4uiv(*av_t, type, coords);
}

void APIENTRY Wrap_glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTexImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, 1, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTexImage1D(*av_t, target, level, internalformat, width, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTexImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTexImage2D(*av_t, target, level, internalformat, width, height, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTexImage2DMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexImage2DMultisample(*av_t, target, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY Wrap_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTexImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTexImage3D(*av_t, target, level, internalformat, width, height, depth, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTexImage3DMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexImage3DMultisample(*av_t, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY Wrap_glTexParameterIiv(GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glTexParameterIiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexParameterIiv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint* params) {
    AV_SCOPE("glTexParameterIuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexParameterIuiv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    AV_SCOPE("glTexParameterf");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexParameterf(*av_t, target, pname, param);
}

void APIENTRY Wrap_glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glTexParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexParameterfv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    AV_SCOPE("glTexParameteri");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexParameteri(*av_t, target, pname, param);
}

void APIENTRY Wrap_glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glTexParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexParameteriv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    AV_SCOPE("glTexStorage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorage1D(*av_t, target, levels, internalformat, width);
}

void APIENTRY Wrap_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glTexStorage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorage2D(*av_t, target, levels, internalformat, width, height);
}

void APIENTRY Wrap_glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTexStorage2DMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorage2DMultisample(*av_t, target, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY Wrap_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    AV_SCOPE("glTexStorage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorage3D(*av_t, target, levels, internalformat, width, height, depth);
}

void APIENTRY Wrap_glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTexStorage3DMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorage3DMultisample(*av_t, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY Wrap_glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTexSubImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, 1, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTexSubImage1D(*av_t, target, level, xoffset, width, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTexSubImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTexSubImage2D(*av_t, target, level, xoffset, yoffset, width, height, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTexSubImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTexSubImage3D(*av_t, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureBarrier(void) {
    AV_SCOPE("glTextureBarrier");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureBarrier(*av_t);
}

void APIENTRY Wrap_glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer) {
    AV_SCOPE("glTextureBuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureBuffer(*av_t, texture, internalformat, buffer);
}

void APIENTRY Wrap_glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    AV_SCOPE("glTextureBufferRange");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureBufferRange(*av_t, texture, internalformat, buffer, offset, size);
}

void APIENTRY Wrap_glTextureParameterIiv(GLuint texture, GLenum pname, const GLint* params) {
    AV_SCOPE("glTextureParameterIiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterIiv(*av_t, texture, pname, params);
}

void APIENTRY Wrap_glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint* params) {
    AV_SCOPE("glTextureParameterIuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterIuiv(*av_t, texture, pname, params);
}

void APIENTRY Wrap_glTextureParameterf(GLuint texture, GLenum pname, GLfloat param) {
    AV_SCOPE("glTextureParameterf");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterf(*av_t, texture, pname, param);
}

void APIENTRY Wrap_glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat* param) {
    AV_SCOPE("glTextureParameterfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterfv(*av_t, texture, pname, param);
}

void APIENTRY Wrap_glTextureParameteri(GLuint texture, GLenum pname, GLint param) {
    AV_SCOPE("glTextureParameteri");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameteri(*av_t, texture, pname, param);
}

void APIENTRY Wrap_glTextureParameteriv(GLuint texture, GLenum pname, const GLint* param) {
    AV_SCOPE("glTextureParameteriv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameteriv(*av_t, texture, pname, param);
}

void APIENTRY Wrap_glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width) {
    AV_SCOPE("glTextureStorage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage1D(*av_t, texture, levels, internalformat, width);
}

void APIENTRY Wrap_glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glTextureStorage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage2D(*av_t, texture, levels, internalformat, width, height);
}

void APIENTRY Wrap_glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTextureStorage2DMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage2DMultisample(*av_t, texture, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY Wrap_glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    AV_SCOPE("glTextureStorage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage3D(*av_t, texture, levels, internalformat, width, height, depth);
}

void APIENTRY Wrap_glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTextureStorage3DMultisample");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage3DMultisample(*av_t, texture, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY Wrap_glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureSubImage1D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, 1, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureSubImage1D(*av_t, texture, level, xoffset, width, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureSubImage2D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureSubImage2D(*av_t, texture, level, xoffset, yoffset, width, height, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureSubImage3D");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureSubImage3D(*av_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers) {
    AV_SCOPE("glTextureView");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureView(*av_t, texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
}

void APIENTRY Wrap_glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer) {
    AV_SCOPE("glTransformFeedbackBufferBase");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glTransformFeedbackBufferBase(*av_t, xfb, index, buffer);
}

void APIENTRY Wrap_glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    AV_SCOPE("glTransformFeedbackBufferRange");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glTransformFeedbackBufferRange(*av_t, xfb, index, buffer, offset, size);
}

void APIENTRY Wrap_glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar*const* varyings, GLenum bufferMode) {
    AV_SCOPE("glTransformFeedbackVaryings");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTransformFeedbackVaryings(*av_t, program, count, varyings, bufferMode);
}

void APIENTRY Wrap_glUniform1d(GLint location, GLdouble x) {
    AV_SCOPE("glUniform1d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1d(*av_t, location, x);
}

void APIENTRY Wrap_glUniform1dv(GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glUniform1dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1dv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform1f(GLint location, GLfloat v0) {
    AV_SCOPE("glUniform1f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1f(*av_t, location, v0);
}

void APIENTRY Wrap_glUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glUniform1fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1fv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform1i(GLint location, GLint v0) {
    AV_SCOPE("glUniform1i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1i(*av_t, location, v0);
}

void APIENTRY Wrap_glUniform1iv(GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glUniform1iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1iv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform1ui(GLint location, GLuint v0) {
    AV_SCOPE("glUniform1ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1ui(*av_t, location, v0);
}

void APIENTRY Wrap_glUniform1uiv(GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glUniform1uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1uiv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform2d(GLint location, GLdouble x, GLdouble y) {
    AV_SCOPE("glUniform2d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2d(*av_t, location, x, y);
}

void APIENTRY Wrap_glUniform2dv(GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glUniform2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2dv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    AV_SCOPE("glUniform2f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2f(*av_t, location, v0, v1);
}

void APIENTRY Wrap_glUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glUniform2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2fv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform2i(GLint location, GLint v0, GLint v1) {
    AV_SCOPE("glUniform2i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2i(*av_t, location, v0, v1);
}

void APIENTRY Wrap_glUniform2iv(GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glUniform2iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2iv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    AV_SCOPE("glUniform2ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2ui(*av_t, location, v0, v1);
}

void APIENTRY Wrap_glUniform2uiv(GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glUniform2uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2uiv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform3d(GLint location, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glUniform3d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3d(*av_t, location, x, y, z);
}

void APIENTRY Wrap_glUniform3dv(GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glUniform3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3dv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    AV_SCOPE("glUniform3f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3f(*av_t, location, v0, v1, v2);
}

void APIENTRY Wrap_glUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glUniform3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3fv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    AV_SCOPE("glUniform3i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3i(*av_t, location, v0, v1, v2);
}

void APIENTRY Wrap_glUniform3iv(GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glUniform3iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3iv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    AV_SCOPE("glUniform3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3ui(*av_t, location, v0, v1, v2);
}

void APIENTRY Wrap_glUniform3uiv(GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glUniform3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3uiv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform4d(GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    AV_SCOPE("glUniform4d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4d(*av_t, location, x, y, z, w);
}

void APIENTRY Wrap_glUniform4dv(GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glUniform4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4dv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    AV_SCOPE("glUniform4f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4f(*av_t, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glUniform4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4fv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    AV_SCOPE("glUniform4i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4i(*av_t, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glUniform4iv(GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glUniform4iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4iv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    AV_SCOPE("glUniform4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4ui(*av_t, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glUniform4uiv(GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glUniform4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4uiv(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    AV_SCOPE("glUniformBlockBinding");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformBlockBinding(*av_t, program, uniformBlockIndex, uniformBlockBinding);
}

void APIENTRY Wrap_glUniformMatrix2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix2dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix2fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix2x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix2x3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix2x3dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix2x3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix2x3fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix2x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix2x4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix2x4dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix2x4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix2x4fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix3dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix3fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix3x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix3x2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix3x2dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix3x2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix3x2fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix3x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix3x4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix3x4dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix3x4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix3x4fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix4dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix4fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix4x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix4x2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix4x2dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix4x2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix4x2fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix4x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glUniformMatrix4x3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix4x3dv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glUniformMatrix4x3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformMatrix4x3fv(*av_t, location, count, transpose, value);
}

void APIENTRY Wrap_glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint* indices) {
    AV_SCOPE("glUniformSubroutinesuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformSubroutinesuiv(*av_t, shadertype, count, indices);
}

void APIENTRY Wrap_glUseProgram(GLuint program) {
    AV_SCOPE("glUseProgram");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUseProgram(*av_t, program);
}

void APIENTRY Wrap_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program) {
    AV_SCOPE("glUseProgramStages");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUseProgramStages(*av_t, pipeline, stages, program);
}

void APIENTRY Wrap_glValidateProgram(GLuint program) {
    AV_SCOPE("glValidateProgram");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glValidateProgram(*av_t, program);
}

void APIENTRY Wrap_glValidateProgramPipeline(GLuint pipeline) {
    AV_SCOPE("glValidateProgramPipeline");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glValidateProgramPipeline(*av_t, pipeline);
}

void APIENTRY Wrap_glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    AV_SCOPE("glVertexArrayAttribBinding");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayAttribBinding(*av_t, vaobj, attribindex, bindingindex);
}

void APIENTRY Wrap_glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    AV_SCOPE("glVertexArrayAttribFormat");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayAttribFormat(*av_t, vaobj, attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY Wrap_glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    AV_SCOPE("glVertexArrayAttribIFormat");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayAttribIFormat(*av_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY Wrap_glVertexArrayAttribLFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    AV_SCOPE("glVertexArrayAttribLFormat");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayAttribLFormat(*av_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY Wrap_glVertexArrayBindingDivisor(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
    AV_SCOPE("glVertexArrayBindingDivisor");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayBindingDivisor(*av_t, vaobj, bindingindex, divisor);
}

void APIENTRY Wrap_glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
    AV_SCOPE("glVertexArrayElementBuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::vertex_array_elements(vaobj, buffer);
    ::av::glproto::av_glVertexArrayElementBuffer(*av_t, vaobj, buffer);
}

void APIENTRY Wrap_glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    AV_SCOPE("glVertexArrayVertexBuffer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexBuffer(*av_t, vaobj, bindingindex, buffer, offset, stride);
}

void APIENTRY Wrap_glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides) {
    AV_SCOPE("glVertexArrayVertexBuffers");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexBuffers(*av_t, vaobj, first, count, buffers, offsets, strides);
}

void APIENTRY Wrap_glVertexAttrib1d(GLuint index, GLdouble x) {
    AV_SCOPE("glVertexAttrib1d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib1d(*av_t, index, x);
}

void APIENTRY Wrap_glVertexAttrib1dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttrib1dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib1dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib1f(GLuint index, GLfloat x) {
    AV_SCOPE("glVertexAttrib1f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib1f(*av_t, index, x);
}

void APIENTRY Wrap_glVertexAttrib1fv(GLuint index, const GLfloat* v) {
    AV_SCOPE("glVertexAttrib1fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib1fv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib1s(GLuint index, GLshort x) {
    AV_SCOPE("glVertexAttrib1s");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib1s(*av_t, index, x);
}

void APIENTRY Wrap_glVertexAttrib1sv(GLuint index, const GLshort* v) {
    AV_SCOPE("glVertexAttrib1sv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib1sv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib2d(GLuint index, GLdouble x, GLdouble y) {
    AV_SCOPE("glVertexAttrib2d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib2d(*av_t, index, x, y);
}

void APIENTRY Wrap_glVertexAttrib2dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttrib2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib2dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    AV_SCOPE("glVertexAttrib2f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib2f(*av_t, index, x, y);
}

void APIENTRY Wrap_glVertexAttrib2fv(GLuint index, const GLfloat* v) {
    AV_SCOPE("glVertexAttrib2fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib2fv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib2s(GLuint index, GLshort x, GLshort y) {
    AV_SCOPE("glVertexAttrib2s");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib2s(*av_t, index, x, y);
}

void APIENTRY Wrap_glVertexAttrib2sv(GLuint index, const GLshort* v) {
    AV_SCOPE("glVertexAttrib2sv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib2sv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glVertexAttrib3d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib3d(*av_t, index, x, y, z);
}

void APIENTRY Wrap_glVertexAttrib3dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttrib3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib3dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glVertexAttrib3f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib3f(*av_t, index, x, y, z);
}

void APIENTRY Wrap_glVertexAttrib3fv(GLuint index, const GLfloat* v) {
    AV_SCOPE("glVertexAttrib3fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib3fv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib3s(GLuint index, GLshort x, GLshort y, GLshort z) {
    AV_SCOPE("glVertexAttrib3s");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib3s(*av_t, index, x, y, z);
}

void APIENTRY Wrap_glVertexAttrib3sv(GLuint index, const GLshort* v) {
    AV_SCOPE("glVertexAttrib3sv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib3sv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4Nbv(GLuint index, const GLbyte* v) {
    AV_SCOPE("glVertexAttrib4Nbv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4Nbv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4Niv(GLuint index, const GLint* v) {
    AV_SCOPE("glVertexAttrib4Niv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4Niv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4Nsv(GLuint index, const GLshort* v) {
    AV_SCOPE("glVertexAttrib4Nsv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4Nsv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {
    AV_SCOPE("glVertexAttrib4Nub");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4Nub(*av_t, index, x, y, z, w);
}

void APIENTRY Wrap_glVertexAttrib4Nubv(GLuint index, const GLubyte* v) {
    AV_SCOPE("glVertexAttrib4Nubv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4Nubv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4Nuiv(GLuint index, const GLuint* v) {
    AV_SCOPE("glVertexAttrib4Nuiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4Nuiv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4Nusv(GLuint index, const GLushort* v) {
    AV_SCOPE("glVertexAttrib4Nusv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4Nusv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4bv(GLuint index, const GLbyte* v) {
    AV_SCOPE("glVertexAttrib4bv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4bv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    AV_SCOPE("glVertexAttrib4d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4d(*av_t, index, x, y, z, w);
}

void APIENTRY Wrap_glVertexAttrib4dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttrib4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    AV_SCOPE("glVertexAttrib4f");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4f(*av_t, index, x, y, z, w);
}

void APIENTRY Wrap_glVertexAttrib4fv(GLuint index, const GLfloat* v) {
    AV_SCOPE("glVertexAttrib4fv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4fv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4iv(GLuint index, const GLint* v) {
    AV_SCOPE("glVertexAttrib4iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4iv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4s(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) {
    AV_SCOPE("glVertexAttrib4s");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4s(*av_t, index, x, y, z, w);
}

void APIENTRY Wrap_glVertexAttrib4sv(GLuint index, const GLshort* v) {
    AV_SCOPE("glVertexAttrib4sv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4sv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4ubv(GLuint index, const GLubyte* v) {
    AV_SCOPE("glVertexAttrib4ubv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4ubv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4uiv(GLuint index, const GLuint* v) {
    AV_SCOPE("glVertexAttrib4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4uiv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttrib4usv(GLuint index, const GLushort* v) {
    AV_SCOPE("glVertexAttrib4usv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttrib4usv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribBinding(GLuint attribindex, GLuint bindingindex) {
    AV_SCOPE("glVertexAttribBinding");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribBinding(*av_t, attribindex, bindingindex);
}

void APIENTRY Wrap_glVertexAttribDivisor(GLuint index, GLuint divisor) {
    AV_SCOPE("glVertexAttribDivisor");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribDivisor(*av_t, index, divisor);
}

void APIENTRY Wrap_glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    AV_SCOPE("glVertexAttribFormat");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribFormat(*av_t, attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY Wrap_glVertexAttribI1i(GLuint index, GLint x) {
    AV_SCOPE("glVertexAttribI1i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI1i(*av_t, index, x);
}

void APIENTRY Wrap_glVertexAttribI1iv(GLuint index, const GLint* v) {
    AV_SCOPE("glVertexAttribI1iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI1iv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI1ui(GLuint index, GLuint x) {
    AV_SCOPE("glVertexAttribI1ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI1ui(*av_t, index, x);
}

void APIENTRY Wrap_glVertexAttribI1uiv(GLuint index, const GLuint* v) {
    AV_SCOPE("glVertexAttribI1uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI1uiv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI2i(GLuint index, GLint x, GLint y) {
    AV_SCOPE("glVertexAttribI2i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI2i(*av_t, index, x, y);
}

void APIENTRY Wrap_glVertexAttribI2iv(GLuint index, const GLint* v) {
    AV_SCOPE("glVertexAttribI2iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI2iv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI2ui(GLuint index, GLuint x, GLuint y) {
    AV_SCOPE("glVertexAttribI2ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI2ui(*av_t, index, x, y);
}

void APIENTRY Wrap_glVertexAttribI2uiv(GLuint index, const GLuint* v) {
    AV_SCOPE("glVertexAttribI2uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI2uiv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z) {
    AV_SCOPE("glVertexAttribI3i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI3i(*av_t, index, x, y, z);
}

void APIENTRY Wrap_glVertexAttribI3iv(GLuint index, const GLint* v) {
    AV_SCOPE("glVertexAttribI3iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI3iv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z) {
    AV_SCOPE("glVertexAttribI3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI3ui(*av_t, index, x, y, z);
}

void APIENTRY Wrap_glVertexAttribI3uiv(GLuint index, const GLuint* v) {
    AV_SCOPE("glVertexAttribI3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI3uiv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI4bv(GLuint index, const GLbyte* v) {
    AV_SCOPE("glVertexAttribI4bv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4bv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    AV_SCOPE("glVertexAttribI4i");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4i(*av_t, index, x, y, z, w);
}

void APIENTRY Wrap_glVertexAttribI4iv(GLuint index, const GLint* v) {
    AV_SCOPE("glVertexAttribI4iv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4iv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI4sv(GLuint index, const GLshort* v) {
    AV_SCOPE("glVertexAttribI4sv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4sv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI4ubv(GLuint index, const GLubyte* v) {
    AV_SCOPE("glVertexAttribI4ubv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4ubv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    AV_SCOPE("glVertexAttribI4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4ui(*av_t, index, x, y, z, w);
}

void APIENTRY Wrap_glVertexAttribI4uiv(GLuint index, const GLuint* v) {
    AV_SCOPE("glVertexAttribI4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4uiv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribI4usv(GLuint index, const GLushort* v) {
    AV_SCOPE("glVertexAttribI4usv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribI4usv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    AV_SCOPE("glVertexAttribIFormat");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribIFormat(*av_t, attribindex, size, type, relativeoffset);
}

void APIENTRY Wrap_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    AV_SCOPE("glVertexAttribIPointer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribIPointer(*av_t, index, size, type, stride, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

void APIENTRY Wrap_glVertexAttribL1d(GLuint index, GLdouble x) {
    AV_SCOPE("glVertexAttribL1d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL1d(*av_t, index, x);
}

void APIENTRY Wrap_glVertexAttribL1dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttribL1dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL1dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribL2d(GLuint index, GLdouble x, GLdouble y) {
    AV_SCOPE("glVertexAttribL2d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL2d(*av_t, index, x, y);
}

void APIENTRY Wrap_glVertexAttribL2dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttribL2dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL2dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glVertexAttribL3d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL3d(*av_t, index, x, y, z);
}

void APIENTRY Wrap_glVertexAttribL3dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttribL3dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL3dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    AV_SCOPE("glVertexAttribL4d");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL4d(*av_t, index, x, y, z, w);
}

void APIENTRY Wrap_glVertexAttribL4dv(GLuint index, const GLdouble* v) {
    AV_SCOPE("glVertexAttribL4dv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL4dv(*av_t, index, v);
}

void APIENTRY Wrap_glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    AV_SCOPE("glVertexAttribLFormat");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribLFormat(*av_t, attribindex, size, type, relativeoffset);
}

void APIENTRY Wrap_glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    AV_SCOPE("glVertexAttribLPointer");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribLPointer(*av_t, index, size, type, stride, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

void APIENTRY Wrap_glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    AV_SCOPE("glVertexAttribP1ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP1ui(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    AV_SCOPE("glVertexAttribP1uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP1uiv(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    AV_SCOPE("glVertexAttribP2ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP2ui(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    AV_SCOPE("glVertexAttribP2uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP2uiv(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    AV_SCOPE("glVertexAttribP3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP3ui(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    AV_SCOPE("glVertexAttribP3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP3uiv(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value) {
    AV_SCOPE("glVertexAttribP4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP4ui(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value) {
    AV_SCOPE("glVertexAttribP4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribP4uiv(*av_t, index, type, normalized, value);
}

void APIENTRY Wrap_glVertexBindingDivisor(GLuint bindingindex, GLuint divisor) {
    AV_SCOPE("glVertexBindingDivisor");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexBindingDivisor(*av_t, bindingindex, divisor);
}

void APIENTRY Wrap_glVertexP2ui(GLenum type, GLuint value) {
    AV_SCOPE("glVertexP2ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexP2ui(*av_t, type, value);
}

void APIENTRY Wrap_glVertexP2uiv(GLenum type, const GLuint* value) {
    AV_SCOPE("glVertexP2uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexP2uiv(*av_t, type, value);
}

void APIENTRY Wrap_glVertexP3ui(GLenum type, GLuint value) {
    AV_SCOPE("glVertexP3ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexP3ui(*av_t, type, value);
}

void APIENTRY Wrap_glVertexP3uiv(GLenum type, const GLuint* value) {
    AV_SCOPE("glVertexP3uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexP3uiv(*av_t, type, value);
}

void APIENTRY Wrap_glVertexP4ui(GLenum type, GLuint value) {
    AV_SCOPE("glVertexP4ui");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexP4ui(*av_t, type, value);
}

void APIENTRY Wrap_glVertexP4uiv(GLenum type, const GLuint* value) {
    AV_SCOPE("glVertexP4uiv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexP4uiv(*av_t, type, value);
}

void APIENTRY Wrap_glViewportArrayv(GLuint first, GLsizei count, const GLfloat* v) {
    AV_SCOPE("glViewportArrayv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glViewportArrayv(*av_t, first, count, v);
}

void APIENTRY Wrap_glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
    AV_SCOPE("glViewportIndexedf");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glViewportIndexedf(*av_t, index, x, y, w, h);
}

void APIENTRY Wrap_glViewportIndexedfv(GLuint index, const GLfloat* v) {
    AV_SCOPE("glViewportIndexedfv");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glViewportIndexedfv(*av_t, index, v);
}

void APIENTRY Wrap_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    AV_SCOPE("glWaitSync");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWaitSync(*av_t, reinterpret_cast<uint64_t>(sync), flags, timeout);
}

void APIENTRY Wrap_glAccum(GLenum op, GLfloat value) {
    AV_SCOPE("glAccum");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glAccum(*av_t, op, value);
}

void APIENTRY Wrap_glAlphaFunc(GLenum func, GLfloat ref) {
    AV_SCOPE("glAlphaFunc");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glAlphaFunc(*av_t, func, ref);
}

GLboolean APIENTRY Wrap_glAreTexturesResident(GLsizei n, const GLuint* textures, GLboolean* residences) {
    AV_SCOPE("glAreTexturesResident");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return 0;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glAreTexturesResident(*av_t, n, textures, residences);
}

void APIENTRY Wrap_glBegin(GLenum mode) {
    AV_SCOPE("glBegin");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBegin(*av_t, mode);
}

void APIENTRY Wrap_glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte* bitmap) {
    AV_SCOPE("glBitmap");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_unpack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glBitmap(*av_t, width, height, xorig, yorig, xmove, ymove, bitmap);
}

void APIENTRY Wrap_glCallList(GLuint list) {
    AV_SCOPE("glCallList");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCallList(*av_t, list);
}

void APIENTRY Wrap_glCallLists(GLsizei n, GLenum type, const void* lists) {
    AV_SCOPE("glCallLists");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCallLists(*av_t, n, type, lists);
}

void APIENTRY Wrap_glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    AV_SCOPE("glClearAccum");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearAccum(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glClearIndex(GLfloat c) {
    AV_SCOPE("glClearIndex");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClearIndex(*av_t, c);
}

void APIENTRY Wrap_glClipPlane(GLenum plane, const GLdouble* equation) {
    AV_SCOPE("glClipPlane");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glClipPlane(*av_t, plane, equation);
}

void APIENTRY Wrap_glColor3b(GLbyte red, GLbyte green, GLbyte blue) {
    AV_SCOPE("glColor3b");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3b(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3bv(const GLbyte* v) {
    AV_SCOPE("glColor3bv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3bv(*av_t, v);
}

void APIENTRY Wrap_glColor3d(GLdouble red, GLdouble green, GLdouble blue) {
    AV_SCOPE("glColor3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3d(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3dv(const GLdouble* v) {
    AV_SCOPE("glColor3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3dv(*av_t, v);
}

void APIENTRY Wrap_glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    AV_SCOPE("glColor3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3f(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3fv(const GLfloat* v) {
    AV_SCOPE("glColor3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3fv(*av_t, v);
}

void APIENTRY Wrap_glColor3i(GLint red, GLint green, GLint blue) {
    AV_SCOPE("glColor3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3i(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3iv(const GLint* v) {
    AV_SCOPE("glColor3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3iv(*av_t, v);
}

void APIENTRY Wrap_glColor3s(GLshort red, GLshort green, GLshort blue) {
    AV_SCOPE("glColor3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3s(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3sv(const GLshort* v) {
    AV_SCOPE("glColor3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3sv(*av_t, v);
}

void APIENTRY Wrap_glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    AV_SCOPE("glColor3ub");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3ub(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3ubv(const GLubyte* v) {
    AV_SCOPE("glColor3ubv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3ubv(*av_t, v);
}

void APIENTRY Wrap_glColor3ui(GLuint red, GLuint green, GLuint blue) {
    AV_SCOPE("glColor3ui");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3ui(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3uiv(const GLuint* v) {
    AV_SCOPE("glColor3uiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3uiv(*av_t, v);
}

void APIENTRY Wrap_glColor3us(GLushort red, GLushort green, GLushort blue) {
    AV_SCOPE("glColor3us");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3us(*av_t, red, green, blue);
}

void APIENTRY Wrap_glColor3usv(const GLushort* v) {
    AV_SCOPE("glColor3usv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor3usv(*av_t, v);
}

void APIENTRY Wrap_glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha) {
    AV_SCOPE("glColor4b");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4b(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4bv(const GLbyte* v) {
    AV_SCOPE("glColor4bv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4bv(*av_t, v);
}

void APIENTRY Wrap_glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha) {
    AV_SCOPE("glColor4d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4d(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4dv(const GLdouble* v) {
    AV_SCOPE("glColor4dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4dv(*av_t, v);
}

void APIENTRY Wrap_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    AV_SCOPE("glColor4f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4f(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4fv(const GLfloat* v) {
    AV_SCOPE("glColor4fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4fv(*av_t, v);
}

void APIENTRY Wrap_glColor4i(GLint red, GLint green, GLint blue, GLint alpha) {
    AV_SCOPE("glColor4i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4i(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4iv(const GLint* v) {
    AV_SCOPE("glColor4iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4iv(*av_t, v);
}

void APIENTRY Wrap_glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha) {
    AV_SCOPE("glColor4s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4s(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4sv(const GLshort* v) {
    AV_SCOPE("glColor4sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4sv(*av_t, v);
}

void APIENTRY Wrap_glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    AV_SCOPE("glColor4ub");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4ub(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4ubv(const GLubyte* v) {
    AV_SCOPE("glColor4ubv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4ubv(*av_t, v);
}

void APIENTRY Wrap_glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha) {
    AV_SCOPE("glColor4ui");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4ui(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4uiv(const GLuint* v) {
    AV_SCOPE("glColor4uiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4uiv(*av_t, v);
}

void APIENTRY Wrap_glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha) {
    AV_SCOPE("glColor4us");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4us(*av_t, red, green, blue, alpha);
}

void APIENTRY Wrap_glColor4usv(const GLushort* v) {
    AV_SCOPE("glColor4usv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColor4usv(*av_t, v);
}

void APIENTRY Wrap_glColorMaterial(GLenum face, GLenum mode) {
    AV_SCOPE("glColorMaterial");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorMaterial(*av_t, face, mode);
}

void APIENTRY Wrap_glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type) {
    AV_SCOPE("glCopyPixels");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyPixels(*av_t, x, y, width, height, type);
}

void APIENTRY Wrap_glDeleteLists(GLuint list, GLsizei range) {
    AV_SCOPE("glDeleteLists");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteLists(*av_t, list, range);
}

void APIENTRY Wrap_glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glDrawPixels");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::flush_persistent_mappings();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glDrawPixels(*av_t, width, height, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glEdgeFlag(GLboolean flag) {
    AV_SCOPE("glEdgeFlag");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEdgeFlag(*av_t, flag);
}

void APIENTRY Wrap_glEdgeFlagv(const GLboolean* flag) {
    AV_SCOPE("glEdgeFlagv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEdgeFlagv(*av_t, flag);
}

void APIENTRY Wrap_glEnd(void) {
    AV_SCOPE("glEnd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnd(*av_t);
}

void APIENTRY Wrap_glEndList(void) {
    AV_SCOPE("glEndList");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEndList(*av_t);
}

void APIENTRY Wrap_glEvalCoord1d(GLdouble u) {
    AV_SCOPE("glEvalCoord1d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord1d(*av_t, u);
}

void APIENTRY Wrap_glEvalCoord1dv(const GLdouble* u) {
    AV_SCOPE("glEvalCoord1dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord1dv(*av_t, u);
}

void APIENTRY Wrap_glEvalCoord1f(GLfloat u) {
    AV_SCOPE("glEvalCoord1f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord1f(*av_t, u);
}

void APIENTRY Wrap_glEvalCoord1fv(const GLfloat* u) {
    AV_SCOPE("glEvalCoord1fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord1fv(*av_t, u);
}

void APIENTRY Wrap_glEvalCoord2d(GLdouble u, GLdouble v) {
    AV_SCOPE("glEvalCoord2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord2d(*av_t, u, v);
}

void APIENTRY Wrap_glEvalCoord2dv(const GLdouble* u) {
    AV_SCOPE("glEvalCoord2dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord2dv(*av_t, u);
}

void APIENTRY Wrap_glEvalCoord2f(GLfloat u, GLfloat v) {
    AV_SCOPE("glEvalCoord2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord2f(*av_t, u, v);
}

void APIENTRY Wrap_glEvalCoord2fv(const GLfloat* u) {
    AV_SCOPE("glEvalCoord2fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalCoord2fv(*av_t, u);
}

void APIENTRY Wrap_glEvalMesh1(GLenum mode, GLint i1, GLint i2) {
    AV_SCOPE("glEvalMesh1");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalMesh1(*av_t, mode, i1, i2);
}

void APIENTRY Wrap_glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) {
    AV_SCOPE("glEvalMesh2");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalMesh2(*av_t, mode, i1, i2, j1, j2);
}

void APIENTRY Wrap_glEvalPoint1(GLint i) {
    AV_SCOPE("glEvalPoint1");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalPoint1(*av_t, i);
}

void APIENTRY Wrap_glEvalPoint2(GLint i, GLint j) {
    AV_SCOPE("glEvalPoint2");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEvalPoint2(*av_t, i, j);
}

void APIENTRY Wrap_glFeedbackBuffer(GLsizei size, GLenum type, GLfloat* buffer) {
    AV_SCOPE("glFeedbackBuffer");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFeedbackBuffer(*av_t, size, type, buffer);
}

void APIENTRY Wrap_glFogCoordd(GLdouble coord) {
    AV_SCOPE("glFogCoordd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogCoordd(*av_t, coord);
}

void APIENTRY Wrap_glFogCoorddv(const GLdouble* coord) {
    AV_SCOPE("glFogCoorddv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogCoorddv(*av_t, coord);
}

void APIENTRY Wrap_glFogCoordf(GLfloat coord) {
    AV_SCOPE("glFogCoordf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogCoordf(*av_t, coord);
}

void APIENTRY Wrap_glFogCoordfv(const GLfloat* coord) {
    AV_SCOPE("glFogCoordfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogCoordfv(*av_t, coord);
}

void APIENTRY Wrap_glFogf(GLenum pname, GLfloat param) {
    AV_SCOPE("glFogf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogf(*av_t, pname, param);
}

void APIENTRY Wrap_glFogfv(GLenum pname, const GLfloat* params) {
    AV_SCOPE("glFogfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogfv(*av_t, pname, params);
}

void APIENTRY Wrap_glFogi(GLenum pname, GLint param) {
    AV_SCOPE("glFogi");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogi(*av_t, pname, param);
}

void APIENTRY Wrap_glFogiv(GLenum pname, const GLint* params) {
    AV_SCOPE("glFogiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogiv(*av_t, pname, params);
}

void APIENTRY Wrap_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    AV_SCOPE("glFrustum");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFrustum(*av_t, left, right, bottom, top, zNear, zFar);
}

GLuint APIENTRY Wrap_glGenLists(GLsizei range) {
    AV_SCOPE("glGenLists");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return 0;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGenLists(*av_t, range);
}

void APIENTRY Wrap_glGetClipPlane(GLenum plane, GLdouble* equation) {
    AV_SCOPE("glGetClipPlane");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetClipPlane(*av_t, plane, equation);
}

void APIENTRY Wrap_glGetLightfv(GLenum light, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetLightfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetLightfv(*av_t, light, pname, params);
}

void APIENTRY Wrap_glGetLightiv(GLenum light, GLenum pname, GLint* params) {
    AV_SCOPE("glGetLightiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetLightiv(*av_t, light, pname, params);
}

void APIENTRY Wrap_glGetMapdv(GLenum target, GLenum query, GLdouble* v) {
    AV_SCOPE("glGetMapdv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMapdv(*av_t, target, query, v);
}

void APIENTRY Wrap_glGetMapfv(GLenum target, GLenum query, GLfloat* v) {
    AV_SCOPE("glGetMapfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMapfv(*av_t, target, query, v);
}

void APIENTRY Wrap_glGetMapiv(GLenum target, GLenum query, GLint* v) {
    AV_SCOPE("glGetMapiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMapiv(*av_t, target, query, v);
}

void APIENTRY Wrap_glGetMaterialfv(GLenum face, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetMaterialfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMaterialfv(*av_t, face, pname, params);
}

void APIENTRY Wrap_glGetMaterialiv(GLenum face, GLenum pname, GLint* params) {
    AV_SCOPE("glGetMaterialiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMaterialiv(*av_t, face, pname, params);
}

void APIENTRY Wrap_glGetPixelMapfv(GLenum map, GLfloat* values) {
    AV_SCOPE("glGetPixelMapfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_pack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetPixelMapfv(*av_t, map, values);
}

void APIENTRY Wrap_glGetPixelMapuiv(GLenum map, GLuint* values) {
    AV_SCOPE("glGetPixelMapuiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_pack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetPixelMapuiv(*av_t, map, values);
}

void APIENTRY Wrap_glGetPixelMapusv(GLenum map, GLushort* values) {
    AV_SCOPE("glGetPixelMapusv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_pack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetPixelMapusv(*av_t, map, values);
}

void APIENTRY Wrap_glGetPolygonStipple(GLubyte* mask) {
    AV_SCOPE("glGetPolygonStipple");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_pack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetPolygonStipple(*av_t, mask);
}

void APIENTRY Wrap_glGetTexEnvfv(GLenum target, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTexEnvfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexEnvfv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetTexEnviv(GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTexEnviv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexEnviv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetTexGendv(GLenum coord, GLenum pname, GLdouble* params) {
    AV_SCOPE("glGetTexGendv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexGendv(*av_t, coord, pname, params);
}

void APIENTRY Wrap_glGetTexGenfv(GLenum coord, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTexGenfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexGenfv(*av_t, coord, pname, params);
}

void APIENTRY Wrap_glGetTexGeniv(GLenum coord, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTexGeniv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTexGeniv(*av_t, coord, pname, params);
}

void APIENTRY Wrap_glIndexMask(GLuint mask) {
    AV_SCOPE("glIndexMask");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexMask(*av_t, mask);
}

void APIENTRY Wrap_glIndexd(GLdouble c) {
    AV_SCOPE("glIndexd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexd(*av_t, c);
}

void APIENTRY Wrap_glIndexdv(GLdouble* c) {
    AV_SCOPE("glIndexdv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexdv(*av_t, c);
}

void APIENTRY Wrap_glIndexf(GLfloat c) {
    AV_SCOPE("glIndexf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexf(*av_t, c);
}

void APIENTRY Wrap_glIndexfv(GLfloat* c) {
    AV_SCOPE("glIndexfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexfv(*av_t, c);
}

void APIENTRY Wrap_glIndexi(GLint c) {
    AV_SCOPE("glIndexi");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexi(*av_t, c);
}

void APIENTRY Wrap_glIndexiv(GLint* c) {
    AV_SCOPE("glIndexiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexiv(*av_t, c);
}

void APIENTRY Wrap_glIndexs(GLshort c) {
    AV_SCOPE("glIndexs");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexs(*av_t, c);
}

void APIENTRY Wrap_glIndexsv(GLshort* c) {
    AV_SCOPE("glIndexsv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexsv(*av_t, c);
}

void APIENTRY Wrap_glIndexub(GLubyte c) {
    AV_SCOPE("glIndexub");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexub(*av_t, c);
}

void APIENTRY Wrap_glIndexubv(GLubyte* c) {
    AV_SCOPE("glIndexubv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexubv(*av_t, c);
}

void APIENTRY Wrap_glInitNames(void) {
    AV_SCOPE("glInitNames");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glInitNames(*av_t);
}

GLboolean APIENTRY Wrap_glIsList(GLuint list) {
    AV_SCOPE("glIsList");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return 0;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsList(*av_t, list);
}

void APIENTRY Wrap_glLightModelf(GLenum pname, GLfloat param) {
    AV_SCOPE("glLightModelf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLightModelf(*av_t, pname, param);
}

void APIENTRY Wrap_glLightModelfv(GLenum pname, const GLfloat* params) {
    AV_SCOPE("glLightModelfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLightModelfv(*av_t, pname, params);
}

void APIENTRY Wrap_glLightModeli(GLenum pname, GLint param) {
    AV_SCOPE("glLightModeli");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLightModeli(*av_t, pname, param);
}

void APIENTRY Wrap_glLightModeliv(GLenum pname, const GLint* params) {
    AV_SCOPE("glLightModeliv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLightModeliv(*av_t, pname, params);
}

void APIENTRY Wrap_glLightf(GLenum light, GLenum pname, GLfloat param) {
    AV_SCOPE("glLightf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLightf(*av_t, light, pname, param);
}

void APIENTRY Wrap_glLightfv(GLenum light, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glLightfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLightfv(*av_t, light, pname, params);
}

void APIENTRY Wrap_glLighti(GLenum light, GLenum pname, GLint param) {
    AV_SCOPE("glLighti");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLighti(*av_t, light, pname, param);
}

void APIENTRY Wrap_glLightiv(GLenum light, GLenum pname, const GLint* params) {
    AV_SCOPE("glLightiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLightiv(*av_t, light, pname, params);
}

void APIENTRY Wrap_glLineStipple(GLint factor, GLushort pattern) {
    AV_SCOPE("glLineStipple");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLineStipple(*av_t, factor, pattern);
}

void APIENTRY Wrap_glListBase(GLuint base) {
    AV_SCOPE("glListBase");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glListBase(*av_t, base);
}

void APIENTRY Wrap_glLoadIdentity(void) {
    AV_SCOPE("glLoadIdentity");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLoadIdentity(*av_t);
}

void APIENTRY Wrap_glLoadMatrixd(const GLdouble* m) {
    AV_SCOPE("glLoadMatrixd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLoadMatrixd(*av_t, m);
}

void APIENTRY Wrap_glLoadMatrixf(const GLfloat* m) {
    AV_SCOPE("glLoadMatrixf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLoadMatrixf(*av_t, m);
}

void APIENTRY Wrap_glLoadName(GLuint name) {
    AV_SCOPE("glLoadName");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLoadName(*av_t, name);
}

void APIENTRY Wrap_glLoadTransposeMatrixd(const GLdouble* m) {
    AV_SCOPE("glLoadTransposeMatrixd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLoadTransposeMatrixd(*av_t, m);
}

void APIENTRY Wrap_glLoadTransposeMatrixf(const GLfloat* m) {
    AV_SCOPE("glLoadTransposeMatrixf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glLoadTransposeMatrixf(*av_t, m);
}

void APIENTRY Wrap_glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble* points) {
    AV_SCOPE("glMap1d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMap1d(*av_t, target, u1, u2, stride, order, points);
}

void APIENTRY Wrap_glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat* points) {
    AV_SCOPE("glMap1f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMap1f(*av_t, target, u1, u2, stride, order, points);
}

void APIENTRY Wrap_glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble* points) {
    AV_SCOPE("glMap2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMap2d(*av_t, target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

void APIENTRY Wrap_glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat* points) {
    AV_SCOPE("glMap2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMap2f(*av_t, target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

void APIENTRY Wrap_glMapGrid1d(GLint un, GLdouble u1, GLdouble u2) {
    AV_SCOPE("glMapGrid1d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMapGrid1d(*av_t, un, u1, u2);
}

void APIENTRY Wrap_glMapGrid1f(GLint un, GLfloat u1, GLfloat u2) {
    AV_SCOPE("glMapGrid1f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMapGrid1f(*av_t, un, u1, u2);
}

void APIENTRY Wrap_glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) {
    AV_SCOPE("glMapGrid2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMapGrid2d(*av_t, un, u1, u2, vn, v1, v2);
}

void APIENTRY Wrap_glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) {
    AV_SCOPE("glMapGrid2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMapGrid2f(*av_t, un, u1, u2, vn, v1, v2);
}

void APIENTRY Wrap_glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    AV_SCOPE("glMaterialf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMaterialf(*av_t, face, pname, param);
}

void APIENTRY Wrap_glMaterialfv(GLenum face, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glMaterialfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMaterialfv(*av_t, face, pname, params);
}

void APIENTRY Wrap_glMateriali(GLenum face, GLenum pname, GLint param) {
    AV_SCOPE("glMateriali");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMateriali(*av_t, face, pname, param);
}

void APIENTRY Wrap_glMaterialiv(GLenum face, GLenum pname, const GLint* params) {
    AV_SCOPE("glMaterialiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMaterialiv(*av_t, face, pname, params);
}

void APIENTRY Wrap_glMatrixMode(GLenum mode) {
    AV_SCOPE("glMatrixMode");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixMode(*av_t, mode);
}

void APIENTRY Wrap_glMultMatrixd(const GLdouble* m) {
    AV_SCOPE("glMultMatrixd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultMatrixd(*av_t, m);
}

void APIENTRY Wrap_glMultMatrixf(const GLfloat* m) {
    AV_SCOPE("glMultMatrixf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultMatrixf(*av_t, m);
}

void APIENTRY Wrap_glMultTransposeMatrixd(const GLdouble* m) {
    AV_SCOPE("glMultTransposeMatrixd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultTransposeMatrixd(*av_t, m);
}

void APIENTRY Wrap_glMultTransposeMatrixf(const GLfloat* m) {
    AV_SCOPE("glMultTransposeMatrixf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultTransposeMatrixf(*av_t, m);
}

void APIENTRY Wrap_glMultiTexCoord1d(GLenum target, GLdouble s) {
    AV_SCOPE("glMultiTexCoord1d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1d(*av_t, target, s);
}

void APIENTRY Wrap_glMultiTexCoord1dv(GLenum target, const GLdouble* v) {
    AV_SCOPE("glMultiTexCoord1dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1dv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord1f(GLenum target, GLfloat s) {
    AV_SCOPE("glMultiTexCoord1f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1f(*av_t, target, s);
}

void APIENTRY Wrap_glMultiTexCoord1fv(GLenum target, const GLfloat* v) {
    AV_SCOPE("glMultiTexCoord1fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1fv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord1i(GLenum target, GLint s) {
    AV_SCOPE("glMultiTexCoord1i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1i(*av_t, target, s);
}

void APIENTRY Wrap_glMultiTexCoord1iv(GLenum target, const GLint* v) {
    AV_SCOPE("glMultiTexCoord1iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1iv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord1s(GLenum target, GLshort s) {
    AV_SCOPE("glMultiTexCoord1s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1s(*av_t, target, s);
}

void APIENTRY Wrap_glMultiTexCoord1sv(GLenum target, const GLshort* v) {
    AV_SCOPE("glMultiTexCoord1sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord1sv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord2d(GLenum target, GLdouble s, GLdouble t) {
    AV_SCOPE("glMultiTexCoord2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2d(*av_t, target, s, t);
}

void APIENTRY Wrap_glMultiTexCoord2dv(GLenum target, const GLdouble* v) {
    AV_SCOPE("glMultiTexCoord2dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2dv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) {
    AV_SCOPE("glMultiTexCoord2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2f(*av_t, target, s, t);
}

void APIENTRY Wrap_glMultiTexCoord2fv(GLenum target, const GLfloat* v) {
    AV_SCOPE("glMultiTexCoord2fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2fv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord2i(GLenum target, GLint s, GLint t) {
    AV_SCOPE("glMultiTexCoord2i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2i(*av_t, target, s, t);
}

void APIENTRY Wrap_glMultiTexCoord2iv(GLenum target, const GLint* v) {
    AV_SCOPE("glMultiTexCoord2iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2iv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord2s(GLenum target, GLshort s, GLshort t) {
    AV_SCOPE("glMultiTexCoord2s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2s(*av_t, target, s, t);
}

void APIENTRY Wrap_glMultiTexCoord2sv(GLenum target, const GLshort* v) {
    AV_SCOPE("glMultiTexCoord2sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord2sv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord3d(GLenum target, GLdouble s, GLdouble t, GLdouble r) {
    AV_SCOPE("glMultiTexCoord3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3d(*av_t, target, s, t, r);
}

void APIENTRY Wrap_glMultiTexCoord3dv(GLenum target, const GLdouble* v) {
    AV_SCOPE("glMultiTexCoord3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3dv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord3f(GLenum target, GLfloat s, GLfloat t, GLfloat r) {
    AV_SCOPE("glMultiTexCoord3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3f(*av_t, target, s, t, r);
}

void APIENTRY Wrap_glMultiTexCoord3fv(GLenum target, const GLfloat* v) {
    AV_SCOPE("glMultiTexCoord3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3fv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord3i(GLenum target, GLint s, GLint t, GLint r) {
    AV_SCOPE("glMultiTexCoord3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3i(*av_t, target, s, t, r);
}

void APIENTRY Wrap_glMultiTexCoord3iv(GLenum target, const GLint* v) {
    AV_SCOPE("glMultiTexCoord3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3iv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord3s(GLenum target, GLshort s, GLshort t, GLshort r) {
    AV_SCOPE("glMultiTexCoord3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3s(*av_t, target, s, t, r);
}

void APIENTRY Wrap_glMultiTexCoord3sv(GLenum target, const GLshort* v) {
    AV_SCOPE("glMultiTexCoord3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord3sv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord4d(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    AV_SCOPE("glMultiTexCoord4d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4d(*av_t, target, s, t, r, q);
}

void APIENTRY Wrap_glMultiTexCoord4dv(GLenum target, const GLdouble* v) {
    AV_SCOPE("glMultiTexCoord4dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4dv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    AV_SCOPE("glMultiTexCoord4f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4f(*av_t, target, s, t, r, q);
}

void APIENTRY Wrap_glMultiTexCoord4fv(GLenum target, const GLfloat* v) {
    AV_SCOPE("glMultiTexCoord4fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4fv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord4i(GLenum target, GLint s, GLint t, GLint r, GLint q) {
    AV_SCOPE("glMultiTexCoord4i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4i(*av_t, target, s, t, r, q);
}

void APIENTRY Wrap_glMultiTexCoord4iv(GLenum target, const GLint* v) {
    AV_SCOPE("glMultiTexCoord4iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4iv(*av_t, target, v);
}

void APIENTRY Wrap_glMultiTexCoord4s(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q) {
    AV_SCOPE("glMultiTexCoord4s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4s(*av_t, target, s, t, r, q);
}

void APIENTRY Wrap_glMultiTexCoord4sv(GLenum target, const GLshort* v) {
    AV_SCOPE("glMultiTexCoord4sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoord4sv(*av_t, target, v);
}

void APIENTRY Wrap_glNewList(GLuint list, GLenum mode) {
    AV_SCOPE("glNewList");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNewList(*av_t, list, mode);
}

void APIENTRY Wrap_glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz) {
    AV_SCOPE("glNormal3b");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3b(*av_t, nx, ny, nz);
}

void APIENTRY Wrap_glNormal3bv(const GLbyte* v) {
    AV_SCOPE("glNormal3bv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3bv(*av_t, v);
}

void APIENTRY Wrap_glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz) {
    AV_SCOPE("glNormal3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3d(*av_t, nx, ny, nz);
}

void APIENTRY Wrap_glNormal3dv(const GLdouble* v) {
    AV_SCOPE("glNormal3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3dv(*av_t, v);
}

void APIENTRY Wrap_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    AV_SCOPE("glNormal3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3f(*av_t, nx, ny, nz);
}

void APIENTRY Wrap_glNormal3fv(const GLfloat* v) {
    AV_SCOPE("glNormal3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3fv(*av_t, v);
}

void APIENTRY Wrap_glNormal3i(GLint nx, GLint ny, GLint nz) {
    AV_SCOPE("glNormal3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3i(*av_t, nx, ny, nz);
}

void APIENTRY Wrap_glNormal3iv(const GLint* v) {
    AV_SCOPE("glNormal3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3iv(*av_t, v);
}

void APIENTRY Wrap_glNormal3s(GLshort nx, GLshort ny, GLshort nz) {
    AV_SCOPE("glNormal3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3s(*av_t, nx, ny, nz);
}

void APIENTRY Wrap_glNormal3sv(const GLshort* v) {
    AV_SCOPE("glNormal3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormal3sv(*av_t, v);
}

void APIENTRY Wrap_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    AV_SCOPE("glOrtho");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glOrtho(*av_t, left, right, bottom, top, zNear, zFar);
}

void APIENTRY Wrap_glPassThrough(GLfloat token) {
    AV_SCOPE("glPassThrough");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPassThrough(*av_t, token);
}

void APIENTRY Wrap_glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat* values) {
    AV_SCOPE("glPixelMapfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_unpack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glPixelMapfv(*av_t, map, mapsize, values);
}

void APIENTRY Wrap_glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint* values) {
    AV_SCOPE("glPixelMapuiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_unpack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glPixelMapuiv(*av_t, map, mapsize, values);
}

void APIENTRY Wrap_glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort* values) {
    AV_SCOPE("glPixelMapusv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_unpack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glPixelMapusv(*av_t, map, mapsize, values);
}

void APIENTRY Wrap_glPixelTransferf(GLenum pname, GLfloat param) {
    AV_SCOPE("glPixelTransferf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPixelTransferf(*av_t, pname, param);
}

void APIENTRY Wrap_glPixelTransferi(GLenum pname, GLint param) {
    AV_SCOPE("glPixelTransferi");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPixelTransferi(*av_t, pname, param);
}

void APIENTRY Wrap_glPixelZoom(GLfloat xfactor, GLfloat yfactor) {
    AV_SCOPE("glPixelZoom");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPixelZoom(*av_t, xfactor, yfactor);
}

void APIENTRY Wrap_glPolygonStipple(const GLubyte* mask) {
    AV_SCOPE("glPolygonStipple");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::refuse_sliced_unpack()) return;
    if (av_t == nullptr) return;
    ::av::glproto::av_glPolygonStipple(*av_t, mask);
}

void APIENTRY Wrap_glPopAttrib(void) {
    AV_SCOPE("glPopAttrib");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPopAttrib(*av_t);
}

void APIENTRY Wrap_glPopClientAttrib(void) {
    AV_SCOPE("glPopClientAttrib");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::client_attributes_popped();
    ::av::glproto::av_glPopClientAttrib(*av_t);
}

void APIENTRY Wrap_glPopMatrix(void) {
    AV_SCOPE("glPopMatrix");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPopMatrix(*av_t);
}

void APIENTRY Wrap_glPopName(void) {
    AV_SCOPE("glPopName");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPopName(*av_t);
}

void APIENTRY Wrap_glPrioritizeTextures(GLsizei n, const GLuint* textures, const GLfloat* priorities) {
    AV_SCOPE("glPrioritizeTextures");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPrioritizeTextures(*av_t, n, textures, priorities);
}

void APIENTRY Wrap_glPushAttrib(GLbitfield mask) {
    AV_SCOPE("glPushAttrib");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPushAttrib(*av_t, mask);
}

void APIENTRY Wrap_glPushClientAttrib(GLbitfield mask) {
    AV_SCOPE("glPushClientAttrib");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::client_attributes_pushed(mask);
    ::av::glproto::av_glPushClientAttrib(*av_t, mask);
}

void APIENTRY Wrap_glPushMatrix(void) {
    AV_SCOPE("glPushMatrix");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPushMatrix(*av_t);
}

void APIENTRY Wrap_glPushName(GLuint name) {
    AV_SCOPE("glPushName");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glPushName(*av_t, name);
}

void APIENTRY Wrap_glRasterPos2d(GLdouble x, GLdouble y) {
    AV_SCOPE("glRasterPos2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2d(*av_t, x, y);
}

void APIENTRY Wrap_glRasterPos2dv(const GLdouble* v) {
    AV_SCOPE("glRasterPos2dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2dv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos2f(GLfloat x, GLfloat y) {
    AV_SCOPE("glRasterPos2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2f(*av_t, x, y);
}

void APIENTRY Wrap_glRasterPos2fv(const GLfloat* v) {
    AV_SCOPE("glRasterPos2fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2fv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos2i(GLint x, GLint y) {
    AV_SCOPE("glRasterPos2i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2i(*av_t, x, y);
}

void APIENTRY Wrap_glRasterPos2iv(const GLint* v) {
    AV_SCOPE("glRasterPos2iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2iv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos2s(GLshort x, GLshort y) {
    AV_SCOPE("glRasterPos2s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2s(*av_t, x, y);
}

void APIENTRY Wrap_glRasterPos2sv(const GLshort* v) {
    AV_SCOPE("glRasterPos2sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos2sv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glRasterPos3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3d(*av_t, x, y, z);
}

void APIENTRY Wrap_glRasterPos3dv(const GLdouble* v) {
    AV_SCOPE("glRasterPos3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3dv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glRasterPos3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3f(*av_t, x, y, z);
}

void APIENTRY Wrap_glRasterPos3fv(const GLfloat* v) {
    AV_SCOPE("glRasterPos3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3fv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos3i(GLint x, GLint y, GLint z) {
    AV_SCOPE("glRasterPos3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3i(*av_t, x, y, z);
}

void APIENTRY Wrap_glRasterPos3iv(const GLint* v) {
    AV_SCOPE("glRasterPos3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3iv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos3s(GLshort x, GLshort y, GLshort z) {
    AV_SCOPE("glRasterPos3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3s(*av_t, x, y, z);
}

void APIENTRY Wrap_glRasterPos3sv(const GLshort* v) {
    AV_SCOPE("glRasterPos3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos3sv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    AV_SCOPE("glRasterPos4d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4d(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glRasterPos4dv(const GLdouble* v) {
    AV_SCOPE("glRasterPos4dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4dv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    AV_SCOPE("glRasterPos4f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4f(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glRasterPos4fv(const GLfloat* v) {
    AV_SCOPE("glRasterPos4fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4fv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos4i(GLint x, GLint y, GLint z, GLint w) {
    AV_SCOPE("glRasterPos4i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4i(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glRasterPos4iv(const GLint* v) {
    AV_SCOPE("glRasterPos4iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4iv(*av_t, v);
}

void APIENTRY Wrap_glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    AV_SCOPE("glRasterPos4s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4s(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glRasterPos4sv(const GLshort* v) {
    AV_SCOPE("glRasterPos4sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRasterPos4sv(*av_t, v);
}

void APIENTRY Wrap_glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) {
    AV_SCOPE("glRectd");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRectd(*av_t, x1, y1, x2, y2);
}

void APIENTRY Wrap_glRectdv(const GLdouble* v1, const GLdouble* v2) {
    AV_SCOPE("glRectdv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRectdv(*av_t, v1, v2);
}

void APIENTRY Wrap_glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
    AV_SCOPE("glRectf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRectf(*av_t, x1, y1, x2, y2);
}

void APIENTRY Wrap_glRectfv(const GLfloat* v1, const GLfloat* v2) {
    AV_SCOPE("glRectfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRectfv(*av_t, v1, v2);
}

void APIENTRY Wrap_glRecti(GLint x1, GLint y1, GLint x2, GLint y2) {
    AV_SCOPE("glRecti");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRecti(*av_t, x1, y1, x2, y2);
}

void APIENTRY Wrap_glRectiv(const GLint* v1, const GLint* v2) {
    AV_SCOPE("glRectiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRectiv(*av_t, v1, v2);
}

void APIENTRY Wrap_glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2) {
    AV_SCOPE("glRects");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRects(*av_t, x1, y1, x2, y2);
}

void APIENTRY Wrap_glRectsv(const GLshort* v1, const GLshort* v2) {
    AV_SCOPE("glRectsv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRectsv(*av_t, v1, v2);
}

GLint APIENTRY Wrap_glRenderMode(GLenum mode) {
    AV_SCOPE("glRenderMode");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return 0;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glRenderMode(*av_t, mode);
}

void APIENTRY Wrap_glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glRotated");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRotated(*av_t, angle, x, y, z);
}

void APIENTRY Wrap_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glRotatef");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glRotatef(*av_t, angle, x, y, z);
}

void APIENTRY Wrap_glScaled(GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glScaled");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScaled(*av_t, x, y, z);
}

void APIENTRY Wrap_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glScalef");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScalef(*av_t, x, y, z);
}

void APIENTRY Wrap_glSecondaryColor3b(GLbyte red, GLbyte green, GLbyte blue) {
    AV_SCOPE("glSecondaryColor3b");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3b(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3bv(const GLbyte* v) {
    AV_SCOPE("glSecondaryColor3bv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3bv(*av_t, v);
}

void APIENTRY Wrap_glSecondaryColor3d(GLdouble red, GLdouble green, GLdouble blue) {
    AV_SCOPE("glSecondaryColor3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3d(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3dv(const GLdouble* v) {
    AV_SCOPE("glSecondaryColor3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3dv(*av_t, v);
}

void APIENTRY Wrap_glSecondaryColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    AV_SCOPE("glSecondaryColor3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3f(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3fv(const GLfloat* v) {
    AV_SCOPE("glSecondaryColor3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3fv(*av_t, v);
}

void APIENTRY Wrap_glSecondaryColor3i(GLint red, GLint green, GLint blue) {
    AV_SCOPE("glSecondaryColor3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3i(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3iv(const GLint* v) {
    AV_SCOPE("glSecondaryColor3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3iv(*av_t, v);
}

void APIENTRY Wrap_glSecondaryColor3s(GLshort red, GLshort green, GLshort blue) {
    AV_SCOPE("glSecondaryColor3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3s(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3sv(const GLshort* v) {
    AV_SCOPE("glSecondaryColor3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3sv(*av_t, v);
}

void APIENTRY Wrap_glSecondaryColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    AV_SCOPE("glSecondaryColor3ub");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3ub(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3ubv(const GLubyte* v) {
    AV_SCOPE("glSecondaryColor3ubv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3ubv(*av_t, v);
}

void APIENTRY Wrap_glSecondaryColor3ui(GLuint red, GLuint green, GLuint blue) {
    AV_SCOPE("glSecondaryColor3ui");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3ui(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3uiv(const GLuint* v) {
    AV_SCOPE("glSecondaryColor3uiv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3uiv(*av_t, v);
}

void APIENTRY Wrap_glSecondaryColor3us(GLushort red, GLushort green, GLushort blue) {
    AV_SCOPE("glSecondaryColor3us");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3us(*av_t, red, green, blue);
}

void APIENTRY Wrap_glSecondaryColor3usv(const GLushort* v) {
    AV_SCOPE("glSecondaryColor3usv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColor3usv(*av_t, v);
}

void APIENTRY Wrap_glSelectBuffer(GLsizei size, GLuint* buffer) {
    AV_SCOPE("glSelectBuffer");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSelectBuffer(*av_t, size, buffer);
}

void APIENTRY Wrap_glShadeModel(GLenum mode) {
    AV_SCOPE("glShadeModel");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glShadeModel(*av_t, mode);
}

void APIENTRY Wrap_glTexCoord1d(GLdouble s) {
    AV_SCOPE("glTexCoord1d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1d(*av_t, s);
}

void APIENTRY Wrap_glTexCoord1dv(const GLdouble* v) {
    AV_SCOPE("glTexCoord1dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1dv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord1f(GLfloat s) {
    AV_SCOPE("glTexCoord1f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1f(*av_t, s);
}

void APIENTRY Wrap_glTexCoord1fv(const GLfloat* v) {
    AV_SCOPE("glTexCoord1fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1fv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord1i(GLint s) {
    AV_SCOPE("glTexCoord1i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1i(*av_t, s);
}

void APIENTRY Wrap_glTexCoord1iv(const GLint* v) {
    AV_SCOPE("glTexCoord1iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1iv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord1s(GLshort s) {
    AV_SCOPE("glTexCoord1s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1s(*av_t, s);
}

void APIENTRY Wrap_glTexCoord1sv(const GLshort* v) {
    AV_SCOPE("glTexCoord1sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord1sv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord2d(GLdouble s, GLdouble t) {
    AV_SCOPE("glTexCoord2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2d(*av_t, s, t);
}

void APIENTRY Wrap_glTexCoord2dv(const GLdouble* v) {
    AV_SCOPE("glTexCoord2dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2dv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord2f(GLfloat s, GLfloat t) {
    AV_SCOPE("glTexCoord2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2f(*av_t, s, t);
}

void APIENTRY Wrap_glTexCoord2fv(const GLfloat* v) {
    AV_SCOPE("glTexCoord2fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2fv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord2i(GLint s, GLint t) {
    AV_SCOPE("glTexCoord2i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2i(*av_t, s, t);
}

void APIENTRY Wrap_glTexCoord2iv(const GLint* v) {
    AV_SCOPE("glTexCoord2iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2iv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord2s(GLshort s, GLshort t) {
    AV_SCOPE("glTexCoord2s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2s(*av_t, s, t);
}

void APIENTRY Wrap_glTexCoord2sv(const GLshort* v) {
    AV_SCOPE("glTexCoord2sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord2sv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) {
    AV_SCOPE("glTexCoord3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3d(*av_t, s, t, r);
}

void APIENTRY Wrap_glTexCoord3dv(const GLdouble* v) {
    AV_SCOPE("glTexCoord3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3dv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {
    AV_SCOPE("glTexCoord3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3f(*av_t, s, t, r);
}

void APIENTRY Wrap_glTexCoord3fv(const GLfloat* v) {
    AV_SCOPE("glTexCoord3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3fv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord3i(GLint s, GLint t, GLint r) {
    AV_SCOPE("glTexCoord3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3i(*av_t, s, t, r);
}

void APIENTRY Wrap_glTexCoord3iv(const GLint* v) {
    AV_SCOPE("glTexCoord3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3iv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord3s(GLshort s, GLshort t, GLshort r) {
    AV_SCOPE("glTexCoord3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3s(*av_t, s, t, r);
}

void APIENTRY Wrap_glTexCoord3sv(const GLshort* v) {
    AV_SCOPE("glTexCoord3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord3sv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) {
    AV_SCOPE("glTexCoord4d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4d(*av_t, s, t, r, q);
}

void APIENTRY Wrap_glTexCoord4dv(const GLdouble* v) {
    AV_SCOPE("glTexCoord4dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4dv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    AV_SCOPE("glTexCoord4f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4f(*av_t, s, t, r, q);
}

void APIENTRY Wrap_glTexCoord4fv(const GLfloat* v) {
    AV_SCOPE("glTexCoord4fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4fv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord4i(GLint s, GLint t, GLint r, GLint q) {
    AV_SCOPE("glTexCoord4i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4i(*av_t, s, t, r, q);
}

void APIENTRY Wrap_glTexCoord4iv(const GLint* v) {
    AV_SCOPE("glTexCoord4iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4iv(*av_t, v);
}

void APIENTRY Wrap_glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) {
    AV_SCOPE("glTexCoord4s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4s(*av_t, s, t, r, q);
}

void APIENTRY Wrap_glTexCoord4sv(const GLshort* v) {
    AV_SCOPE("glTexCoord4sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoord4sv(*av_t, v);
}

void APIENTRY Wrap_glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    AV_SCOPE("glTexEnvf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexEnvf(*av_t, target, pname, param);
}

void APIENTRY Wrap_glTexEnvfv(GLenum target, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glTexEnvfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexEnvfv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glTexEnvi(GLenum target, GLenum pname, GLint param) {
    AV_SCOPE("glTexEnvi");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexEnvi(*av_t, target, pname, param);
}

void APIENTRY Wrap_glTexEnviv(GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glTexEnviv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexEnviv(*av_t, target, pname, params);
}

void APIENTRY Wrap_glTexGend(GLenum coord, GLenum pname, GLdouble param) {
    AV_SCOPE("glTexGend");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexGend(*av_t, coord, pname, param);
}

void APIENTRY Wrap_glTexGendv(GLenum coord, GLenum pname, const GLdouble* params) {
    AV_SCOPE("glTexGendv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexGendv(*av_t, coord, pname, params);
}

void APIENTRY Wrap_glTexGenf(GLenum coord, GLenum pname, GLfloat param) {
    AV_SCOPE("glTexGenf");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexGenf(*av_t, coord, pname, param);
}

void APIENTRY Wrap_glTexGenfv(GLenum coord, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glTexGenfv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexGenfv(*av_t, coord, pname, params);
}

void APIENTRY Wrap_glTexGeni(GLenum coord, GLenum pname, GLint param) {
    AV_SCOPE("glTexGeni");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexGeni(*av_t, coord, pname, param);
}

void APIENTRY Wrap_glTexGeniv(GLenum coord, GLenum pname, const GLint* params) {
    AV_SCOPE("glTexGeniv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexGeniv(*av_t, coord, pname, params);
}

void APIENTRY Wrap_glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glTranslated");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTranslated(*av_t, x, y, z);
}

void APIENTRY Wrap_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glTranslatef");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTranslatef(*av_t, x, y, z);
}

void APIENTRY Wrap_glVertex2d(GLdouble x, GLdouble y) {
    AV_SCOPE("glVertex2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2d(*av_t, x, y);
}

void APIENTRY Wrap_glVertex2dv(const GLdouble* v) {
    AV_SCOPE("glVertex2dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2dv(*av_t, v);
}

void APIENTRY Wrap_glVertex2f(GLfloat x, GLfloat y) {
    AV_SCOPE("glVertex2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2f(*av_t, x, y);
}

void APIENTRY Wrap_glVertex2fv(const GLfloat* v) {
    AV_SCOPE("glVertex2fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2fv(*av_t, v);
}

void APIENTRY Wrap_glVertex2i(GLint x, GLint y) {
    AV_SCOPE("glVertex2i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2i(*av_t, x, y);
}

void APIENTRY Wrap_glVertex2iv(const GLint* v) {
    AV_SCOPE("glVertex2iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2iv(*av_t, v);
}

void APIENTRY Wrap_glVertex2s(GLshort x, GLshort y) {
    AV_SCOPE("glVertex2s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2s(*av_t, x, y);
}

void APIENTRY Wrap_glVertex2sv(const GLshort* v) {
    AV_SCOPE("glVertex2sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex2sv(*av_t, v);
}

void APIENTRY Wrap_glVertex3d(GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glVertex3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3d(*av_t, x, y, z);
}

void APIENTRY Wrap_glVertex3dv(const GLdouble* v) {
    AV_SCOPE("glVertex3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3dv(*av_t, v);
}

void APIENTRY Wrap_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glVertex3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3f(*av_t, x, y, z);
}

void APIENTRY Wrap_glVertex3fv(const GLfloat* v) {
    AV_SCOPE("glVertex3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3fv(*av_t, v);
}

void APIENTRY Wrap_glVertex3i(GLint x, GLint y, GLint z) {
    AV_SCOPE("glVertex3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3i(*av_t, x, y, z);
}

void APIENTRY Wrap_glVertex3iv(const GLint* v) {
    AV_SCOPE("glVertex3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3iv(*av_t, v);
}

void APIENTRY Wrap_glVertex3s(GLshort x, GLshort y, GLshort z) {
    AV_SCOPE("glVertex3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3s(*av_t, x, y, z);
}

void APIENTRY Wrap_glVertex3sv(const GLshort* v) {
    AV_SCOPE("glVertex3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex3sv(*av_t, v);
}

void APIENTRY Wrap_glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    AV_SCOPE("glVertex4d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4d(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glVertex4dv(const GLdouble* v) {
    AV_SCOPE("glVertex4dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4dv(*av_t, v);
}

void APIENTRY Wrap_glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    AV_SCOPE("glVertex4f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4f(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glVertex4fv(const GLfloat* v) {
    AV_SCOPE("glVertex4fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4fv(*av_t, v);
}

void APIENTRY Wrap_glVertex4i(GLint x, GLint y, GLint z, GLint w) {
    AV_SCOPE("glVertex4i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4i(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glVertex4iv(const GLint* v) {
    AV_SCOPE("glVertex4iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4iv(*av_t, v);
}

void APIENTRY Wrap_glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) {
    AV_SCOPE("glVertex4s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4s(*av_t, x, y, z, w);
}

void APIENTRY Wrap_glVertex4sv(const GLshort* v) {
    AV_SCOPE("glVertex4sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertex4sv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos2d(GLdouble x, GLdouble y) {
    AV_SCOPE("glWindowPos2d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2d(*av_t, x, y);
}

void APIENTRY Wrap_glWindowPos2dv(const GLdouble* v) {
    AV_SCOPE("glWindowPos2dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2dv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos2f(GLfloat x, GLfloat y) {
    AV_SCOPE("glWindowPos2f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2f(*av_t, x, y);
}

void APIENTRY Wrap_glWindowPos2fv(const GLfloat* v) {
    AV_SCOPE("glWindowPos2fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2fv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos2i(GLint x, GLint y) {
    AV_SCOPE("glWindowPos2i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2i(*av_t, x, y);
}

void APIENTRY Wrap_glWindowPos2iv(const GLint* v) {
    AV_SCOPE("glWindowPos2iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2iv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos2s(GLshort x, GLshort y) {
    AV_SCOPE("glWindowPos2s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2s(*av_t, x, y);
}

void APIENTRY Wrap_glWindowPos2sv(const GLshort* v) {
    AV_SCOPE("glWindowPos2sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos2sv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos3d(GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glWindowPos3d");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3d(*av_t, x, y, z);
}

void APIENTRY Wrap_glWindowPos3dv(const GLdouble* v) {
    AV_SCOPE("glWindowPos3dv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3dv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos3f(GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glWindowPos3f");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3f(*av_t, x, y, z);
}

void APIENTRY Wrap_glWindowPos3fv(const GLfloat* v) {
    AV_SCOPE("glWindowPos3fv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3fv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos3i(GLint x, GLint y, GLint z) {
    AV_SCOPE("glWindowPos3i");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3i(*av_t, x, y, z);
}

void APIENTRY Wrap_glWindowPos3iv(const GLint* v) {
    AV_SCOPE("glWindowPos3iv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3iv(*av_t, v);
}

void APIENTRY Wrap_glWindowPos3s(GLshort x, GLshort y, GLshort z) {
    AV_SCOPE("glWindowPos3s");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3s(*av_t, x, y, z);
}

void APIENTRY Wrap_glWindowPos3sv(const GLshort* v) {
    AV_SCOPE("glWindowPos3sv");
    if (!::av::glicd::legacy_calls_live()) {
        ::av::glicd::refuse_legacy();
        return;
    }
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWindowPos3sv(*av_t, v);
}

void APIENTRY Wrap_glDeleteObjectARB(GLhandleARB obj) {
    AV_SCOPE("glDeleteObjectARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteObjectARB(*av_t, obj);
}

GLhandleARB APIENTRY Wrap_glGetHandleARB(GLenum pname) {
    AV_SCOPE("glGetHandleARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetHandleARB(*av_t, pname);
}

void APIENTRY Wrap_glGetObjectParameterfvARB(GLhandleARB obj, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetObjectParameterfvARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetObjectParameterfvARB(*av_t, obj, pname, params);
}

void APIENTRY Wrap_glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint* params) {
    AV_SCOPE("glGetObjectParameterivARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetObjectParameterivARB(*av_t, obj, pname, params);
}

void APIENTRY Wrap_glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei* length, GLcharARB* infoLog) {
    AV_SCOPE("glGetInfoLogARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetInfoLogARB(*av_t, obj, maxLength, length, infoLog);
}

void APIENTRY Wrap_glGetAttachedObjectsARB(GLhandleARB containerObj, GLsizei maxCount, GLsizei* count, GLhandleARB* obj) {
    AV_SCOPE("glGetAttachedObjectsARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetAttachedObjectsARB(*av_t, containerObj, maxCount, count, obj);
}

void APIENTRY Wrap_glTexPageCommitmentARB(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit) {
    AV_SCOPE("glTexPageCommitmentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexPageCommitmentARB(*av_t, target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void APIENTRY Wrap_glBufferPageCommitmentARB(GLenum target, GLintptr offset, GLsizeiptr size, GLboolean commit) {
    AV_SCOPE("glBufferPageCommitmentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_target(target);
    ::av::glproto::av_glBufferPageCommitmentARB(*av_t, target, offset, size, commit);
}

void APIENTRY Wrap_glNamedBufferPageCommitmentEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit) {
    AV_SCOPE("glNamedBufferPageCommitmentEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glNamedBufferPageCommitmentEXT(*av_t, buffer, offset, size, commit);
}

void APIENTRY Wrap_glNamedBufferPageCommitmentARB(GLuint buffer, GLintptr offset, GLsizeiptr size, GLboolean commit) {
    AV_SCOPE("glNamedBufferPageCommitmentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glNamedBufferPageCommitmentARB(*av_t, buffer, offset, size, commit);
}

void APIENTRY Wrap_glTexturePageCommitmentEXT(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit) {
    AV_SCOPE("glTexturePageCommitmentEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexturePageCommitmentEXT(*av_t, texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void APIENTRY Wrap_glMaxShaderCompilerThreadsARB(GLuint count) {
    AV_SCOPE("glMaxShaderCompilerThreadsARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMaxShaderCompilerThreadsARB(*av_t, count);
}

void APIENTRY Wrap_glMaxShaderCompilerThreadsKHR(GLuint count) {
    AV_SCOPE("glMaxShaderCompilerThreadsKHR");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMaxShaderCompilerThreadsKHR(*av_t, count);
}

void APIENTRY Wrap_glScissorExclusiveNV(GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glScissorExclusiveNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScissorExclusiveNV(*av_t, x, y, width, height);
}

void APIENTRY Wrap_glScissorExclusiveArrayvNV(GLuint first, GLsizei count, const GLint* v) {
    AV_SCOPE("glScissorExclusiveArrayvNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glScissorExclusiveArrayvNV(*av_t, first, count, v);
}

void APIENTRY Wrap_glFramebufferSampleLocationsfvNV(GLenum target, GLuint start, GLsizei count, const GLfloat* v) {
    AV_SCOPE("glFramebufferSampleLocationsfvNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferSampleLocationsfvNV(*av_t, target, start, count, v);
}

void APIENTRY Wrap_glNamedFramebufferSampleLocationsfvNV(GLuint framebuffer, GLuint start, GLsizei count, const GLfloat* v) {
    AV_SCOPE("glNamedFramebufferSampleLocationsfvNV");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferSampleLocationsfvNV(*av_t, framebuffer, start, count, v);
}

void APIENTRY Wrap_glResolveDepthValuesNV(void) {
    AV_SCOPE("glResolveDepthValuesNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glResolveDepthValuesNV(*av_t);
}

GLuint64 APIENTRY Wrap_glGetTextureHandleARB(GLuint texture) {
    AV_SCOPE("glGetTextureHandleARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetTextureHandleARB(*av_t, texture);
}

GLuint64 APIENTRY Wrap_glGetTextureSamplerHandleARB(GLuint texture, GLuint sampler) {
    AV_SCOPE("glGetTextureSamplerHandleARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetTextureSamplerHandleARB(*av_t, texture, sampler);
}

void APIENTRY Wrap_glMakeTextureHandleResidentARB(GLuint64 handle) {
    AV_SCOPE("glMakeTextureHandleResidentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeTextureHandleResidentARB(*av_t, handle);
}

void APIENTRY Wrap_glMakeTextureHandleNonResidentARB(GLuint64 handle) {
    AV_SCOPE("glMakeTextureHandleNonResidentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeTextureHandleNonResidentARB(*av_t, handle);
}

GLuint64 APIENTRY Wrap_glGetImageHandleARB(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format) {
    AV_SCOPE("glGetImageHandleARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetImageHandleARB(*av_t, texture, level, layered, layer, format);
}

void APIENTRY Wrap_glMakeImageHandleResidentARB(GLuint64 handle, GLenum access) {
    AV_SCOPE("glMakeImageHandleResidentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glMakeImageHandleResidentARB(*av_t, handle, access);
}

void APIENTRY Wrap_glMakeImageHandleNonResidentARB(GLuint64 handle) {
    AV_SCOPE("glMakeImageHandleNonResidentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeImageHandleNonResidentARB(*av_t, handle);
}

void APIENTRY Wrap_glUniformHandleui64ARB(GLint location, GLuint64 value) {
    AV_SCOPE("glUniformHandleui64ARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformHandleui64ARB(*av_t, location, value);
}

void APIENTRY Wrap_glUniformHandleui64vARB(GLint location, GLsizei count, const GLuint64* value) {
    AV_SCOPE("glUniformHandleui64vARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformHandleui64vARB(*av_t, location, count, value);
}

void APIENTRY Wrap_glProgramUniformHandleui64ARB(GLuint program, GLint location, GLuint64 value) {
    AV_SCOPE("glProgramUniformHandleui64ARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformHandleui64ARB(*av_t, program, location, value);
}

void APIENTRY Wrap_glProgramUniformHandleui64vARB(GLuint program, GLint location, GLsizei count, const GLuint64* values) {
    AV_SCOPE("glProgramUniformHandleui64vARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformHandleui64vARB(*av_t, program, location, count, values);
}

GLboolean APIENTRY Wrap_glIsTextureHandleResidentARB(GLuint64 handle) {
    AV_SCOPE("glIsTextureHandleResidentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsTextureHandleResidentARB(*av_t, handle);
}

GLboolean APIENTRY Wrap_glIsImageHandleResidentARB(GLuint64 handle) {
    AV_SCOPE("glIsImageHandleResidentARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsImageHandleResidentARB(*av_t, handle);
}

void APIENTRY Wrap_glVertexAttribL1ui64ARB(GLuint index, GLuint64EXT x) {
    AV_SCOPE("glVertexAttribL1ui64ARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL1ui64ARB(*av_t, index, x);
}

void APIENTRY Wrap_glVertexAttribL1ui64vARB(GLuint index, const GLuint64EXT* v) {
    AV_SCOPE("glVertexAttribL1ui64vARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribL1ui64vARB(*av_t, index, v);
}

void APIENTRY Wrap_glGetVertexAttribLui64vARB(GLuint index, GLenum pname, GLuint64EXT* params) {
    AV_SCOPE("glGetVertexAttribLui64vARB");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexAttribLui64vARB(*av_t, index, pname, params);
}

GLuint64 APIENTRY Wrap_glGetTextureHandleNV(GLuint texture) {
    AV_SCOPE("glGetTextureHandleNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetTextureHandleNV(*av_t, texture);
}

GLuint64 APIENTRY Wrap_glGetTextureSamplerHandleNV(GLuint texture, GLuint sampler) {
    AV_SCOPE("glGetTextureSamplerHandleNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetTextureSamplerHandleNV(*av_t, texture, sampler);
}

void APIENTRY Wrap_glMakeTextureHandleResidentNV(GLuint64 handle) {
    AV_SCOPE("glMakeTextureHandleResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeTextureHandleResidentNV(*av_t, handle);
}

void APIENTRY Wrap_glMakeTextureHandleNonResidentNV(GLuint64 handle) {
    AV_SCOPE("glMakeTextureHandleNonResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeTextureHandleNonResidentNV(*av_t, handle);
}

GLuint64 APIENTRY Wrap_glGetImageHandleNV(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format) {
    AV_SCOPE("glGetImageHandleNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glGetImageHandleNV(*av_t, texture, level, layered, layer, format);
}

void APIENTRY Wrap_glMakeImageHandleResidentNV(GLuint64 handle, GLenum access) {
    AV_SCOPE("glMakeImageHandleResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glMakeImageHandleResidentNV(*av_t, handle, access);
}

void APIENTRY Wrap_glMakeImageHandleNonResidentNV(GLuint64 handle) {
    AV_SCOPE("glMakeImageHandleNonResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeImageHandleNonResidentNV(*av_t, handle);
}

void APIENTRY Wrap_glUniformHandleui64NV(GLint location, GLuint64 value) {
    AV_SCOPE("glUniformHandleui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformHandleui64NV(*av_t, location, value);
}

void APIENTRY Wrap_glUniformHandleui64vNV(GLint location, GLsizei count, const GLuint64* value) {
    AV_SCOPE("glUniformHandleui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformHandleui64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glProgramUniformHandleui64NV(GLuint program, GLint location, GLuint64 value) {
    AV_SCOPE("glProgramUniformHandleui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformHandleui64NV(*av_t, program, location, value);
}

void APIENTRY Wrap_glProgramUniformHandleui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64* values) {
    AV_SCOPE("glProgramUniformHandleui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformHandleui64vNV(*av_t, program, location, count, values);
}

GLboolean APIENTRY Wrap_glIsTextureHandleResidentNV(GLuint64 handle) {
    AV_SCOPE("glIsTextureHandleResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsTextureHandleResidentNV(*av_t, handle);
}

GLboolean APIENTRY Wrap_glIsImageHandleResidentNV(GLuint64 handle) {
    AV_SCOPE("glIsImageHandleResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsImageHandleResidentNV(*av_t, handle);
}

void APIENTRY Wrap_glBufferStorageMemEXT(GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glBufferStorageMemEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBufferStorageMemEXT(*av_t, target, size, memory, offset);
}

void APIENTRY Wrap_glTextureStorageMem2DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTextureStorageMem2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorageMem2DEXT(*av_t, texture, levels, internalFormat, width, height, memory, offset);
}

void APIENTRY Wrap_glDeleteMemoryObjectsEXT(GLsizei n, const GLuint* memoryObjects) {
    AV_SCOPE("glDeleteMemoryObjectsEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteMemoryObjectsEXT(*av_t, n, memoryObjects);
}

void APIENTRY Wrap_glDeleteSemaphoresEXT(GLsizei n, const GLuint* semaphores) {
    AV_SCOPE("glDeleteSemaphoresEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDeleteSemaphoresEXT(*av_t, n, semaphores);
}

void APIENTRY Wrap_glSignalSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint* buffers, GLuint numTextureBarriers, const GLuint* textures, const GLenum* dstLayouts) {
    AV_SCOPE("glSignalSemaphoreEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSignalSemaphoreEXT(*av_t, semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, dstLayouts);
}

void APIENTRY Wrap_glWaitSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint* buffers, GLuint numTextureBarriers, const GLuint* textures, const GLenum* srcLayouts) {
    AV_SCOPE("glWaitSemaphoreEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glWaitSemaphoreEXT(*av_t, semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, srcLayouts);
}

void APIENTRY Wrap_glTexStorageMem1DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTexStorageMem1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorageMem1DEXT(*av_t, target, levels, internalFormat, width, memory, offset);
}

void APIENTRY Wrap_glTexStorageMem2DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTexStorageMem2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorageMem2DEXT(*av_t, target, levels, internalFormat, width, height, memory, offset);
}

void APIENTRY Wrap_glTexStorageMem2DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTexStorageMem2DMultisampleEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorageMem2DMultisampleEXT(*av_t, target, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
}

void APIENTRY Wrap_glTexStorageMem3DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTexStorageMem3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorageMem3DEXT(*av_t, target, levels, internalFormat, width, height, depth, memory, offset);
}

void APIENTRY Wrap_glTexStorageMem3DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTexStorageMem3DMultisampleEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexStorageMem3DMultisampleEXT(*av_t, target, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
}

void APIENTRY Wrap_glTextureStorageMem1DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTextureStorageMem1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorageMem1DEXT(*av_t, texture, levels, internalFormat, width, memory, offset);
}

void APIENTRY Wrap_glTextureStorageMem2DMultisampleEXT(GLuint texture, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTextureStorageMem2DMultisampleEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorageMem2DMultisampleEXT(*av_t, texture, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
}

void APIENTRY Wrap_glTextureStorageMem3DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTextureStorageMem3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorageMem3DEXT(*av_t, texture, levels, internalFormat, width, height, depth, memory, offset);
}

void APIENTRY Wrap_glTextureStorageMem3DMultisampleEXT(GLuint texture, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glTextureStorageMem3DMultisampleEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorageMem3DMultisampleEXT(*av_t, texture, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
}

void APIENTRY Wrap_glNamedBufferStorageMemEXT(GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset) {
    AV_SCOPE("glNamedBufferStorageMemEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedBufferStorageMemEXT(*av_t, buffer, size, memory, offset);
}

void APIENTRY Wrap_glGetUnsignedBytevEXT(GLenum pname, GLubyte* data) {
    AV_SCOPE("glGetUnsignedBytevEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUnsignedBytevEXT(*av_t, pname, data);
}

void APIENTRY Wrap_glGetUnsignedBytei_vEXT(GLenum target, GLuint index, GLubyte* data) {
    AV_SCOPE("glGetUnsignedBytei_vEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUnsignedBytei_vEXT(*av_t, target, index, data);
}

void APIENTRY Wrap_glMakeBufferResidentNV(GLenum target, GLenum access) {
    AV_SCOPE("glMakeBufferResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glMakeBufferResidentNV(*av_t, target, access);
}

void APIENTRY Wrap_glMakeBufferNonResidentNV(GLenum target) {
    AV_SCOPE("glMakeBufferNonResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeBufferNonResidentNV(*av_t, target);
}

GLboolean APIENTRY Wrap_glIsBufferResidentNV(GLenum target) {
    AV_SCOPE("glIsBufferResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsBufferResidentNV(*av_t, target);
}

void APIENTRY Wrap_glMakeNamedBufferResidentNV(GLuint buffer, GLenum access) {
    AV_SCOPE("glMakeNamedBufferResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::shaders_write_buffers();
    ::av::glproto::av_glMakeNamedBufferResidentNV(*av_t, buffer, access);
}

void APIENTRY Wrap_glMakeNamedBufferNonResidentNV(GLuint buffer) {
    AV_SCOPE("glMakeNamedBufferNonResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMakeNamedBufferNonResidentNV(*av_t, buffer);
}

GLboolean APIENTRY Wrap_glIsNamedBufferResidentNV(GLuint buffer) {
    AV_SCOPE("glIsNamedBufferResidentNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsNamedBufferResidentNV(*av_t, buffer);
}

void APIENTRY Wrap_glGetBufferParameterui64vNV(GLenum target, GLenum pname, GLuint64EXT* params) {
    AV_SCOPE("glGetBufferParameterui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetBufferParameterui64vNV(*av_t, target, pname, params);
}

void APIENTRY Wrap_glGetNamedBufferParameterui64vNV(GLuint buffer, GLenum pname, GLuint64EXT* params) {
    AV_SCOPE("glGetNamedBufferParameterui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedBufferParameterui64vNV(*av_t, buffer, pname, params);
}

void APIENTRY Wrap_glGetIntegerui64vNV(GLenum value, GLuint64EXT* result) {
    AV_SCOPE("glGetIntegerui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetIntegerui64vNV(*av_t, value, result);
}

void APIENTRY Wrap_glUniformui64NV(GLint location, GLuint64EXT value) {
    AV_SCOPE("glUniformui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformui64NV(*av_t, location, value);
}

void APIENTRY Wrap_glUniformui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glUniformui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniformui64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glGetUniformui64vNV(GLuint program, GLint location, GLuint64EXT* params) {
    AV_SCOPE("glGetUniformui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformui64vNV(*av_t, program, location, params);
}

void APIENTRY Wrap_glProgramUniformui64NV(GLuint program, GLint location, GLuint64EXT value) {
    AV_SCOPE("glProgramUniformui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformui64NV(*av_t, program, location, value);
}

void APIENTRY Wrap_glProgramUniformui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glProgramUniformui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformui64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glBufferAddressRangeNV(GLenum pname, GLuint index, GLuint64EXT address, GLsizeiptr length) {
    AV_SCOPE("glBufferAddressRangeNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBufferAddressRangeNV(*av_t, pname, index, address, length);
}

void APIENTRY Wrap_glVertexFormatNV(GLint size, GLenum type, GLsizei stride) {
    AV_SCOPE("glVertexFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexFormatNV(*av_t, size, type, stride);
}

void APIENTRY Wrap_glNormalFormatNV(GLenum type, GLsizei stride) {
    AV_SCOPE("glNormalFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNormalFormatNV(*av_t, type, stride);
}

void APIENTRY Wrap_glColorFormatNV(GLint size, GLenum type, GLsizei stride) {
    AV_SCOPE("glColorFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glColorFormatNV(*av_t, size, type, stride);
}

void APIENTRY Wrap_glIndexFormatNV(GLenum type, GLsizei stride) {
    AV_SCOPE("glIndexFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glIndexFormatNV(*av_t, type, stride);
}

void APIENTRY Wrap_glTexCoordFormatNV(GLint size, GLenum type, GLsizei stride) {
    AV_SCOPE("glTexCoordFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTexCoordFormatNV(*av_t, size, type, stride);
}

void APIENTRY Wrap_glEdgeFlagFormatNV(GLsizei stride) {
    AV_SCOPE("glEdgeFlagFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEdgeFlagFormatNV(*av_t, stride);
}

void APIENTRY Wrap_glSecondaryColorFormatNV(GLint size, GLenum type, GLsizei stride) {
    AV_SCOPE("glSecondaryColorFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glSecondaryColorFormatNV(*av_t, size, type, stride);
}

void APIENTRY Wrap_glFogCoordFormatNV(GLenum type, GLsizei stride) {
    AV_SCOPE("glFogCoordFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glFogCoordFormatNV(*av_t, type, stride);
}

void APIENTRY Wrap_glVertexAttribFormatNV(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride) {
    AV_SCOPE("glVertexAttribFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribFormatNV(*av_t, index, size, type, normalized, stride);
}

void APIENTRY Wrap_glVertexAttribIFormatNV(GLuint index, GLint size, GLenum type, GLsizei stride) {
    AV_SCOPE("glVertexAttribIFormatNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexAttribIFormatNV(*av_t, index, size, type, stride);
}

void APIENTRY Wrap_glGetIntegerui64i_vNV(GLenum value, GLuint index, GLuint64EXT* result) {
    AV_SCOPE("glGetIntegerui64i_vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetIntegerui64i_vNV(*av_t, value, index, result);
}

void APIENTRY Wrap_glUniform1i64NV(GLint location, GLint64EXT x) {
    AV_SCOPE("glUniform1i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1i64NV(*av_t, location, x);
}

void APIENTRY Wrap_glUniform2i64NV(GLint location, GLint64EXT x, GLint64EXT y) {
    AV_SCOPE("glUniform2i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2i64NV(*av_t, location, x, y);
}

void APIENTRY Wrap_glUniform3i64NV(GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z) {
    AV_SCOPE("glUniform3i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3i64NV(*av_t, location, x, y, z);
}

void APIENTRY Wrap_glUniform4i64NV(GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w) {
    AV_SCOPE("glUniform4i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4i64NV(*av_t, location, x, y, z, w);
}

void APIENTRY Wrap_glUniform1i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glUniform1i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1i64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform2i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glUniform2i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2i64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform3i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glUniform3i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3i64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform4i64vNV(GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glUniform4i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4i64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform1ui64NV(GLint location, GLuint64EXT x) {
    AV_SCOPE("glUniform1ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1ui64NV(*av_t, location, x);
}

void APIENTRY Wrap_glUniform2ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y) {
    AV_SCOPE("glUniform2ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2ui64NV(*av_t, location, x, y);
}

void APIENTRY Wrap_glUniform3ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z) {
    AV_SCOPE("glUniform3ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3ui64NV(*av_t, location, x, y, z);
}

void APIENTRY Wrap_glUniform4ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w) {
    AV_SCOPE("glUniform4ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4ui64NV(*av_t, location, x, y, z, w);
}

void APIENTRY Wrap_glUniform1ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glUniform1ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform1ui64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform2ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glUniform2ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform2ui64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform3ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glUniform3ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform3ui64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glUniform4ui64vNV(GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glUniform4ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glUniform4ui64vNV(*av_t, location, count, value);
}

void APIENTRY Wrap_glGetUniformi64vNV(GLuint program, GLint location, GLint64EXT* params) {
    AV_SCOPE("glGetUniformi64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetUniformi64vNV(*av_t, program, location, params);
}

void APIENTRY Wrap_glProgramUniform1i64NV(GLuint program, GLint location, GLint64EXT x) {
    AV_SCOPE("glProgramUniform1i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1i64NV(*av_t, program, location, x);
}

void APIENTRY Wrap_glProgramUniform2i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y) {
    AV_SCOPE("glProgramUniform2i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2i64NV(*av_t, program, location, x, y);
}

void APIENTRY Wrap_glProgramUniform3i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z) {
    AV_SCOPE("glProgramUniform3i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3i64NV(*av_t, program, location, x, y, z);
}

void APIENTRY Wrap_glProgramUniform4i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w) {
    AV_SCOPE("glProgramUniform4i64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4i64NV(*av_t, program, location, x, y, z, w);
}

void APIENTRY Wrap_glProgramUniform1i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glProgramUniform1i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1i64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glProgramUniform2i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2i64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glProgramUniform3i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3i64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT* value) {
    AV_SCOPE("glProgramUniform4i64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4i64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform1ui64NV(GLuint program, GLint location, GLuint64EXT x) {
    AV_SCOPE("glProgramUniform1ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1ui64NV(*av_t, program, location, x);
}

void APIENTRY Wrap_glProgramUniform2ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y) {
    AV_SCOPE("glProgramUniform2ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2ui64NV(*av_t, program, location, x, y);
}

void APIENTRY Wrap_glProgramUniform3ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z) {
    AV_SCOPE("glProgramUniform3ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3ui64NV(*av_t, program, location, x, y, z);
}

void APIENTRY Wrap_glProgramUniform4ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w) {
    AV_SCOPE("glProgramUniform4ui64NV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4ui64NV(*av_t, program, location, x, y, z, w);
}

void APIENTRY Wrap_glProgramUniform1ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glProgramUniform1ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1ui64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glProgramUniform2ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2ui64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glProgramUniform3ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3ui64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT* value) {
    AV_SCOPE("glProgramUniform4ui64vNV");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4ui64vNV(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glMatrixLoadfEXT(GLenum mode, const GLfloat* m) {
    AV_SCOPE("glMatrixLoadfEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixLoadfEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glMatrixLoaddEXT(GLenum mode, const GLdouble* m) {
    AV_SCOPE("glMatrixLoaddEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixLoaddEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glMatrixMultfEXT(GLenum mode, const GLfloat* m) {
    AV_SCOPE("glMatrixMultfEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixMultfEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glMatrixMultdEXT(GLenum mode, const GLdouble* m) {
    AV_SCOPE("glMatrixMultdEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixMultdEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glMatrixLoadIdentityEXT(GLenum mode) {
    AV_SCOPE("glMatrixLoadIdentityEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixLoadIdentityEXT(*av_t, mode);
}

void APIENTRY Wrap_glMatrixRotatefEXT(GLenum mode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glMatrixRotatefEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixRotatefEXT(*av_t, mode, angle, x, y, z);
}

void APIENTRY Wrap_glMatrixRotatedEXT(GLenum mode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glMatrixRotatedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixRotatedEXT(*av_t, mode, angle, x, y, z);
}

void APIENTRY Wrap_glMatrixScalefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glMatrixScalefEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixScalefEXT(*av_t, mode, x, y, z);
}

void APIENTRY Wrap_glMatrixScaledEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glMatrixScaledEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixScaledEXT(*av_t, mode, x, y, z);
}

void APIENTRY Wrap_glMatrixTranslatefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    AV_SCOPE("glMatrixTranslatefEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixTranslatefEXT(*av_t, mode, x, y, z);
}

void APIENTRY Wrap_glMatrixTranslatedEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glMatrixTranslatedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixTranslatedEXT(*av_t, mode, x, y, z);
}

void APIENTRY Wrap_glMatrixFrustumEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    AV_SCOPE("glMatrixFrustumEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixFrustumEXT(*av_t, mode, left, right, bottom, top, zNear, zFar);
}

void APIENTRY Wrap_glMatrixOrthoEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    AV_SCOPE("glMatrixOrthoEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixOrthoEXT(*av_t, mode, left, right, bottom, top, zNear, zFar);
}

void APIENTRY Wrap_glMatrixPopEXT(GLenum mode) {
    AV_SCOPE("glMatrixPopEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixPopEXT(*av_t, mode);
}

void APIENTRY Wrap_glMatrixPushEXT(GLenum mode) {
    AV_SCOPE("glMatrixPushEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixPushEXT(*av_t, mode);
}

void APIENTRY Wrap_glClientAttribDefaultEXT(GLbitfield mask) {
    AV_SCOPE("glClientAttribDefaultEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::client_attributes_reset(mask);
    ::av::glproto::av_glClientAttribDefaultEXT(*av_t, mask);
}

void APIENTRY Wrap_glPushClientAttribDefaultEXT(GLbitfield mask) {
    AV_SCOPE("glPushClientAttribDefaultEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::client_attributes_pushed(mask); ::av::glicd::client_attributes_reset(mask);
    ::av::glproto::av_glPushClientAttribDefaultEXT(*av_t, mask);
}

void APIENTRY Wrap_glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param) {
    AV_SCOPE("glTextureParameterfEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterfEXT(*av_t, texture, target, pname, param);
}

void APIENTRY Wrap_glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glTextureParameterfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterfvEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param) {
    AV_SCOPE("glTextureParameteriEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameteriEXT(*av_t, texture, target, pname, param);
}

void APIENTRY Wrap_glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glTextureParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterivEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, 1, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureImage1DEXT(*av_t, texture, target, level, internalformat, width, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureImage2DEXT(*av_t, texture, target, level, internalformat, width, height, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureSubImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, 1, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureSubImage1DEXT(*av_t, texture, target, level, xoffset, width, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureSubImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureSubImage2DEXT(*av_t, texture, target, level, xoffset, yoffset, width, height, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCopyTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    AV_SCOPE("glCopyTextureImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureImage1DEXT(*av_t, texture, target, level, internalformat, x, y, width, border);
}

void APIENTRY Wrap_glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    AV_SCOPE("glCopyTextureImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureImage2DEXT(*av_t, texture, target, level, internalformat, x, y, width, height, border);
}

void APIENTRY Wrap_glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    AV_SCOPE("glCopyTextureSubImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureSubImage1DEXT(*av_t, texture, target, level, xoffset, x, y, width);
}

void APIENTRY Wrap_glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyTextureSubImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureSubImage2DEXT(*av_t, texture, target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY Wrap_glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    AV_SCOPE("glGetTextureImageEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::level_bytes(target, level, format, type);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetTextureImageEXT(*av_t, texture, target, level, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTextureParameterfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameterfvEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTextureParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameterivEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetTextureLevelParameterfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureLevelParameterfvEXT(*av_t, texture, target, level, pname, params);
}

void APIENTRY Wrap_glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTextureLevelParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureLevelParameterivEXT(*av_t, texture, target, level, pname, params);
}

void APIENTRY Wrap_glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureImage3DEXT(*av_t, texture, target, level, internalformat, width, height, depth, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glTextureSubImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glTextureSubImage3DEXT(*av_t, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyTextureSubImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyTextureSubImage3DEXT(*av_t, texture, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY Wrap_glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture) {
    AV_SCOPE("glBindMultiTextureEXT");
    auto* av_t = ::av::glicd::wire();
    ::av::glicd::forget_bindings();
    if (av_t == nullptr) return;
    ::av::glproto::av_glBindMultiTextureEXT(*av_t, texunit, target, texture);
}

void APIENTRY Wrap_glMultiTexCoordPointerEXT(GLenum texunit, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    AV_SCOPE("glMultiTexCoordPointerEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexCoordPointerEXT(*av_t, texunit, size, type, stride, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

void APIENTRY Wrap_glMultiTexEnvfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    AV_SCOPE("glMultiTexEnvfEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexEnvfEXT(*av_t, texunit, target, pname, param);
}

void APIENTRY Wrap_glMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glMultiTexEnvfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexEnvfvEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glMultiTexEnviEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    AV_SCOPE("glMultiTexEnviEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexEnviEXT(*av_t, texunit, target, pname, param);
}

void APIENTRY Wrap_glMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glMultiTexEnvivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexEnvivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glMultiTexGendEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble param) {
    AV_SCOPE("glMultiTexGendEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexGendEXT(*av_t, texunit, coord, pname, param);
}

void APIENTRY Wrap_glMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLdouble* params) {
    AV_SCOPE("glMultiTexGendvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexGendvEXT(*av_t, texunit, coord, pname, params);
}

void APIENTRY Wrap_glMultiTexGenfEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat param) {
    AV_SCOPE("glMultiTexGenfEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexGenfEXT(*av_t, texunit, coord, pname, param);
}

void APIENTRY Wrap_glMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glMultiTexGenfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexGenfvEXT(*av_t, texunit, coord, pname, params);
}

void APIENTRY Wrap_glMultiTexGeniEXT(GLenum texunit, GLenum coord, GLenum pname, GLint param) {
    AV_SCOPE("glMultiTexGeniEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexGeniEXT(*av_t, texunit, coord, pname, param);
}

void APIENTRY Wrap_glMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, const GLint* params) {
    AV_SCOPE("glMultiTexGenivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexGenivEXT(*av_t, texunit, coord, pname, params);
}

void APIENTRY Wrap_glGetMultiTexEnvfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetMultiTexEnvfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexEnvfvEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glGetMultiTexEnvivEXT(GLenum texunit, GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetMultiTexEnvivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexEnvivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glGetMultiTexGendvEXT(GLenum texunit, GLenum coord, GLenum pname, GLdouble* params) {
    AV_SCOPE("glGetMultiTexGendvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexGendvEXT(*av_t, texunit, coord, pname, params);
}

void APIENTRY Wrap_glGetMultiTexGenfvEXT(GLenum texunit, GLenum coord, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetMultiTexGenfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexGenfvEXT(*av_t, texunit, coord, pname, params);
}

void APIENTRY Wrap_glGetMultiTexGenivEXT(GLenum texunit, GLenum coord, GLenum pname, GLint* params) {
    AV_SCOPE("glGetMultiTexGenivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexGenivEXT(*av_t, texunit, coord, pname, params);
}

void APIENTRY Wrap_glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param) {
    AV_SCOPE("glMultiTexParameteriEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexParameteriEXT(*av_t, texunit, target, pname, param);
}

void APIENTRY Wrap_glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glMultiTexParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexParameterivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param) {
    AV_SCOPE("glMultiTexParameterfEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexParameterfEXT(*av_t, texunit, target, pname, param);
}

void APIENTRY Wrap_glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat* params) {
    AV_SCOPE("glMultiTexParameterfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexParameterfvEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glMultiTexImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, 1, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glMultiTexImage1DEXT(*av_t, texunit, target, level, internalformat, width, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glMultiTexImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glMultiTexImage2DEXT(*av_t, texunit, target, level, internalformat, width, height, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glMultiTexSubImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, 1, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glMultiTexSubImage1DEXT(*av_t, texunit, target, level, xoffset, width, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glMultiTexSubImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, 1, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glMultiTexSubImage2DEXT(*av_t, texunit, target, level, xoffset, yoffset, width, height, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCopyMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) {
    AV_SCOPE("glCopyMultiTexImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyMultiTexImage1DEXT(*av_t, texunit, target, level, internalformat, x, y, width, border);
}

void APIENTRY Wrap_glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    AV_SCOPE("glCopyMultiTexImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyMultiTexImage2DEXT(*av_t, texunit, target, level, internalformat, x, y, width, height, border);
}

void APIENTRY Wrap_glCopyMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    AV_SCOPE("glCopyMultiTexSubImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyMultiTexSubImage1DEXT(*av_t, texunit, target, level, xoffset, x, y, width);
}

void APIENTRY Wrap_glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyMultiTexSubImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyMultiTexSubImage2DEXT(*av_t, texunit, target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY Wrap_glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    AV_SCOPE("glGetMultiTexImageEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::pack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::level_bytes(target, level, format, type);
    if (av_pixels_slice && !::av::glicd::slice_pack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_download(pixels, av_pixels_bytes);
    if (av_pixels_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetMultiTexImageEXT(*av_t, texunit, target, level, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_download(pixels, av_pixels_bytes);
    if (av_pixels_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetMultiTexParameterfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexParameterfvEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetMultiTexParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexParameterivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLfloat* params) {
    AV_SCOPE("glGetMultiTexLevelParameterfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexLevelParameterfvEXT(*av_t, texunit, target, level, pname, params);
}

void APIENTRY Wrap_glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLint* params) {
    AV_SCOPE("glGetMultiTexLevelParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexLevelParameterivEXT(*av_t, texunit, target, level, pname, params);
}

void APIENTRY Wrap_glMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glMultiTexImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glMultiTexImage3DEXT(*av_t, texunit, target, level, internalformat, width, height, depth, border, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    AV_SCOPE("glMultiTexSubImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_pixels_slice = ::av::glicd::unpack_slice_bound();
    const bool av_pixels_offset = !av_pixels_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_pixels_bytes = av_pixels_offset ? 0 : ::av::glicd::pixel_bytes(format, type, width, height, depth, false);
    if (av_pixels_slice && !::av::glicd::slice_unpack_pixels(pixels, av_pixels_bytes)) return;
    const bool av_pixels_staged = !av_pixels_offset && ::av::glicd::stage_upload(pixels, av_pixels_bytes);
    ::av::glproto::av_glMultiTexSubImage3DEXT(*av_t, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, av_pixels_staged ? nullptr : pixels, av_pixels_staged ? 0 : av_pixels_bytes, av_pixels_offset || av_pixels_staged);
    if (av_pixels_staged) ::av::glicd::finish_upload();
    if (av_pixels_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    AV_SCOPE("glCopyMultiTexSubImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCopyMultiTexSubImage3DEXT(*av_t, texunit, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY Wrap_glEnableClientStateIndexedEXT(GLenum array, GLuint index) {
    AV_SCOPE("glEnableClientStateIndexedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnableClientStateIndexedEXT(*av_t, array, index);
}

void APIENTRY Wrap_glDisableClientStateIndexedEXT(GLenum array, GLuint index) {
    AV_SCOPE("glDisableClientStateIndexedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisableClientStateIndexedEXT(*av_t, array, index);
}

void APIENTRY Wrap_glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat* data) {
    AV_SCOPE("glGetFloatIndexedvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetFloatIndexedvEXT(*av_t, target, index, data);
}

void APIENTRY Wrap_glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble* data) {
    AV_SCOPE("glGetDoubleIndexedvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetDoubleIndexedvEXT(*av_t, target, index, data);
}

void APIENTRY Wrap_glEnableIndexedEXT(GLenum target, GLuint index) {
    AV_SCOPE("glEnableIndexedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnableIndexedEXT(*av_t, target, index);
}

void APIENTRY Wrap_glDisableIndexedEXT(GLenum target, GLuint index) {
    AV_SCOPE("glDisableIndexedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisableIndexedEXT(*av_t, target, index);
}

GLboolean APIENTRY Wrap_glIsEnabledIndexedEXT(GLenum target, GLuint index) {
    AV_SCOPE("glIsEnabledIndexedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glIsEnabledIndexedEXT(*av_t, target, index);
}

void APIENTRY Wrap_glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint* data) {
    AV_SCOPE("glGetIntegerIndexedvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetIntegerIndexedvEXT(*av_t, target, index, data);
}

void APIENTRY Wrap_glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean* data) {
    AV_SCOPE("glGetBooleanIndexedvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetBooleanIndexedvEXT(*av_t, target, index, data);
}

void APIENTRY Wrap_glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedTextureImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedTextureImage3DEXT(*av_t, texture, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void APIENTRY Wrap_glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedTextureImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedTextureImage2DEXT(*av_t, texture, target, level, internalformat, width, height, border, imageSize, bits);
}

void APIENTRY Wrap_glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedTextureImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedTextureImage1DEXT(*av_t, texture, target, level, internalformat, width, border, imageSize, bits);
}

void APIENTRY Wrap_glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedTextureSubImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedTextureSubImage3DEXT(*av_t, texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void APIENTRY Wrap_glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedTextureSubImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedTextureSubImage2DEXT(*av_t, texture, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void APIENTRY Wrap_glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedTextureSubImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedTextureSubImage1DEXT(*av_t, texture, target, level, xoffset, width, format, imageSize, bits);
}

void APIENTRY Wrap_glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint lod, void* img) {
    AV_SCOPE("glGetCompressedTextureImageEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_img_slice = ::av::glicd::pack_slice_bound();
    const bool av_img_offset = !av_img_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_img_bytes = av_img_offset ? 0 : ::av::glicd::compressed_level_bytes(target, lod, true);
    if (av_img_slice && !::av::glicd::slice_pack_pixels(img, av_img_bytes)) return;
    const bool av_img_staged = !av_img_offset && ::av::glicd::stage_download(img, av_img_bytes);
    if (av_img_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetCompressedTextureImageEXT(*av_t, texture, target, lod, av_img_staged ? nullptr : img, av_img_staged ? 0 : av_img_bytes, av_img_offset || av_img_staged);
    if (av_img_staged) ::av::glicd::finish_download(img, av_img_bytes);
    if (av_img_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glCompressedMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedMultiTexImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedMultiTexImage3DEXT(*av_t, texunit, target, level, internalformat, width, height, depth, border, imageSize, bits);
}

void APIENTRY Wrap_glCompressedMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedMultiTexImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedMultiTexImage2DEXT(*av_t, texunit, target, level, internalformat, width, height, border, imageSize, bits);
}

void APIENTRY Wrap_glCompressedMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedMultiTexImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedMultiTexImage1DEXT(*av_t, texunit, target, level, internalformat, width, border, imageSize, bits);
}

void APIENTRY Wrap_glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedMultiTexSubImage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedMultiTexSubImage3DEXT(*av_t, texunit, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, bits);
}

void APIENTRY Wrap_glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedMultiTexSubImage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedMultiTexSubImage2DEXT(*av_t, texunit, target, level, xoffset, yoffset, width, height, format, imageSize, bits);
}

void APIENTRY Wrap_glCompressedMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* bits) {
    AV_SCOPE("glCompressedMultiTexSubImage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glCompressedMultiTexSubImage1DEXT(*av_t, texunit, target, level, xoffset, width, format, imageSize, bits);
}

void APIENTRY Wrap_glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint lod, void* img) {
    AV_SCOPE("glGetCompressedMultiTexImageEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    const bool av_img_slice = ::av::glicd::pack_slice_bound();
    const bool av_img_offset = !av_img_slice && ::av::glicd::pixel_buffer_bound(true);
    const size_t av_img_bytes = av_img_offset ? 0 : ::av::glicd::compressed_level_bytes(target, lod, true);
    if (av_img_slice && !::av::glicd::slice_pack_pixels(img, av_img_bytes)) return;
    const bool av_img_staged = !av_img_offset && ::av::glicd::stage_download(img, av_img_bytes);
    if (av_img_offset) ::av::glicd::host_writes_target(0x88EBu);
    ::av::glproto::av_glGetCompressedMultiTexImageEXT(*av_t, texunit, target, lod, av_img_staged ? nullptr : img, av_img_staged ? 0 : av_img_bytes, av_img_offset || av_img_staged);
    if (av_img_staged) ::av::glicd::finish_download(img, av_img_bytes);
    if (av_img_slice) ::av::glicd::pack_slice_done();
}

void APIENTRY Wrap_glMatrixLoadTransposefEXT(GLenum mode, const GLfloat* m) {
    AV_SCOPE("glMatrixLoadTransposefEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixLoadTransposefEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glMatrixLoadTransposedEXT(GLenum mode, const GLdouble* m) {
    AV_SCOPE("glMatrixLoadTransposedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixLoadTransposedEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glMatrixMultTransposefEXT(GLenum mode, const GLfloat* m) {
    AV_SCOPE("glMatrixMultTransposefEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixMultTransposefEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glMatrixMultTransposedEXT(GLenum mode, const GLdouble* m) {
    AV_SCOPE("glMatrixMultTransposedEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMatrixMultTransposedEXT(*av_t, mode, m);
}

void APIENTRY Wrap_glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedBufferParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    if (::av::glicd::answer_named_buffer_map_state(buffer, pname, params)) return;
    ::av::glproto::av_glGetNamedBufferParameterivEXT(*av_t, buffer, pname, params);
}

void APIENTRY Wrap_glProgramUniform1fEXT(GLuint program, GLint location, GLfloat v0) {
    AV_SCOPE("glProgramUniform1fEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1fEXT(*av_t, program, location, v0);
}

void APIENTRY Wrap_glProgramUniform2fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    AV_SCOPE("glProgramUniform2fEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2fEXT(*av_t, program, location, v0, v1);
}

void APIENTRY Wrap_glProgramUniform3fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    AV_SCOPE("glProgramUniform3fEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3fEXT(*av_t, program, location, v0, v1, v2);
}

void APIENTRY Wrap_glProgramUniform4fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    AV_SCOPE("glProgramUniform4fEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4fEXT(*av_t, program, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glProgramUniform1iEXT(GLuint program, GLint location, GLint v0) {
    AV_SCOPE("glProgramUniform1iEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1iEXT(*av_t, program, location, v0);
}

void APIENTRY Wrap_glProgramUniform2iEXT(GLuint program, GLint location, GLint v0, GLint v1) {
    AV_SCOPE("glProgramUniform2iEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2iEXT(*av_t, program, location, v0, v1);
}

void APIENTRY Wrap_glProgramUniform3iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    AV_SCOPE("glProgramUniform3iEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3iEXT(*av_t, program, location, v0, v1, v2);
}

void APIENTRY Wrap_glProgramUniform4iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    AV_SCOPE("glProgramUniform4iEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4iEXT(*av_t, program, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glProgramUniform1fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform1fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1fvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform2fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2fvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform3fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3fvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat* value) {
    AV_SCOPE("glProgramUniform4fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4fvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform1ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform1ivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1ivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform2ivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2ivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform3ivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3ivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4ivEXT(GLuint program, GLint location, GLsizei count, const GLint* value) {
    AV_SCOPE("glProgramUniform4ivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4ivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix2fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix3fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix4fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix2x3fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x3fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix3x2fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x2fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix2x4fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x4fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix4x2fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x2fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix3x4fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x4fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    AV_SCOPE("glProgramUniformMatrix4x3fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x3fvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer) {
    AV_SCOPE("glTextureBufferEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureBufferEXT(*av_t, texture, target, internalformat, buffer);
}

void APIENTRY Wrap_glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer) {
    AV_SCOPE("glMultiTexBufferEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexBufferEXT(*av_t, texunit, target, internalformat, buffer);
}

void APIENTRY Wrap_glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glTextureParameterIivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterIivEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, const GLuint* params) {
    AV_SCOPE("glTextureParameterIuivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureParameterIuivEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetTextureParameterIivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameterIivEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, GLuint* params) {
    AV_SCOPE("glGetTextureParameterIuivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetTextureParameterIuivEXT(*av_t, texture, target, pname, params);
}

void APIENTRY Wrap_glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint* params) {
    AV_SCOPE("glMultiTexParameterIivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexParameterIivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, const GLuint* params) {
    AV_SCOPE("glMultiTexParameterIuivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexParameterIuivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetMultiTexParameterIivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexParameterIivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, GLuint* params) {
    AV_SCOPE("glGetMultiTexParameterIuivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetMultiTexParameterIuivEXT(*av_t, texunit, target, pname, params);
}

void APIENTRY Wrap_glProgramUniform1uiEXT(GLuint program, GLint location, GLuint v0) {
    AV_SCOPE("glProgramUniform1uiEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1uiEXT(*av_t, program, location, v0);
}

void APIENTRY Wrap_glProgramUniform2uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1) {
    AV_SCOPE("glProgramUniform2uiEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2uiEXT(*av_t, program, location, v0, v1);
}

void APIENTRY Wrap_glProgramUniform3uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    AV_SCOPE("glProgramUniform3uiEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3uiEXT(*av_t, program, location, v0, v1, v2);
}

void APIENTRY Wrap_glProgramUniform4uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    AV_SCOPE("glProgramUniform4uiEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4uiEXT(*av_t, program, location, v0, v1, v2, v3);
}

void APIENTRY Wrap_glProgramUniform1uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform1uivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1uivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform2uivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2uivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform3uivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3uivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4uivEXT(GLuint program, GLint location, GLsizei count, const GLuint* value) {
    AV_SCOPE("glProgramUniform4uivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4uivEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glNamedProgramLocalParameters4fvEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLfloat* params) {
    AV_SCOPE("glNamedProgramLocalParameters4fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameters4fvEXT(*av_t, program, target, index, count, params);
}

void APIENTRY Wrap_glNamedProgramLocalParameterI4iEXT(GLuint program, GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w) {
    AV_SCOPE("glNamedProgramLocalParameterI4iEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameterI4iEXT(*av_t, program, target, index, x, y, z, w);
}

void APIENTRY Wrap_glNamedProgramLocalParameterI4ivEXT(GLuint program, GLenum target, GLuint index, const GLint* params) {
    AV_SCOPE("glNamedProgramLocalParameterI4ivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameterI4ivEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glNamedProgramLocalParametersI4ivEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLint* params) {
    AV_SCOPE("glNamedProgramLocalParametersI4ivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParametersI4ivEXT(*av_t, program, target, index, count, params);
}

void APIENTRY Wrap_glNamedProgramLocalParameterI4uiEXT(GLuint program, GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    AV_SCOPE("glNamedProgramLocalParameterI4uiEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameterI4uiEXT(*av_t, program, target, index, x, y, z, w);
}

void APIENTRY Wrap_glNamedProgramLocalParameterI4uivEXT(GLuint program, GLenum target, GLuint index, const GLuint* params) {
    AV_SCOPE("glNamedProgramLocalParameterI4uivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameterI4uivEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glNamedProgramLocalParametersI4uivEXT(GLuint program, GLenum target, GLuint index, GLsizei count, const GLuint* params) {
    AV_SCOPE("glNamedProgramLocalParametersI4uivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParametersI4uivEXT(*av_t, program, target, index, count, params);
}

void APIENTRY Wrap_glGetNamedProgramLocalParameterIivEXT(GLuint program, GLenum target, GLuint index, GLint* params) {
    AV_SCOPE("glGetNamedProgramLocalParameterIivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedProgramLocalParameterIivEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glGetNamedProgramLocalParameterIuivEXT(GLuint program, GLenum target, GLuint index, GLuint* params) {
    AV_SCOPE("glGetNamedProgramLocalParameterIuivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedProgramLocalParameterIuivEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glEnableClientStateiEXT(GLenum array, GLuint index) {
    AV_SCOPE("glEnableClientStateiEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnableClientStateiEXT(*av_t, array, index);
}

void APIENTRY Wrap_glDisableClientStateiEXT(GLenum array, GLuint index) {
    AV_SCOPE("glDisableClientStateiEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisableClientStateiEXT(*av_t, array, index);
}

void APIENTRY Wrap_glGetFloati_vEXT(GLenum pname, GLuint index, GLfloat* params) {
    AV_SCOPE("glGetFloati_vEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetFloati_vEXT(*av_t, pname, index, params);
}

void APIENTRY Wrap_glGetDoublei_vEXT(GLenum pname, GLuint index, GLdouble* params) {
    AV_SCOPE("glGetDoublei_vEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetDoublei_vEXT(*av_t, pname, index, params);
}

void APIENTRY Wrap_glNamedProgramStringEXT(GLuint program, GLenum target, GLenum format, GLsizei len, const void* string) {
    AV_SCOPE("glNamedProgramStringEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramStringEXT(*av_t, program, target, format, len, string);
}

void APIENTRY Wrap_glNamedProgramLocalParameter4dEXT(GLuint program, GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    AV_SCOPE("glNamedProgramLocalParameter4dEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameter4dEXT(*av_t, program, target, index, x, y, z, w);
}

void APIENTRY Wrap_glNamedProgramLocalParameter4dvEXT(GLuint program, GLenum target, GLuint index, const GLdouble* params) {
    AV_SCOPE("glNamedProgramLocalParameter4dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameter4dvEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glNamedProgramLocalParameter4fEXT(GLuint program, GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    AV_SCOPE("glNamedProgramLocalParameter4fEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameter4fEXT(*av_t, program, target, index, x, y, z, w);
}

void APIENTRY Wrap_glNamedProgramLocalParameter4fvEXT(GLuint program, GLenum target, GLuint index, const GLfloat* params) {
    AV_SCOPE("glNamedProgramLocalParameter4fvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedProgramLocalParameter4fvEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glGetNamedProgramLocalParameterdvEXT(GLuint program, GLenum target, GLuint index, GLdouble* params) {
    AV_SCOPE("glGetNamedProgramLocalParameterdvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedProgramLocalParameterdvEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glGetNamedProgramLocalParameterfvEXT(GLuint program, GLenum target, GLuint index, GLfloat* params) {
    AV_SCOPE("glGetNamedProgramLocalParameterfvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedProgramLocalParameterfvEXT(*av_t, program, target, index, params);
}

void APIENTRY Wrap_glGetNamedProgramivEXT(GLuint program, GLenum target, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedProgramivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedProgramivEXT(*av_t, program, target, pname, params);
}

void APIENTRY Wrap_glGetNamedProgramStringEXT(GLuint program, GLenum target, GLenum pname, void* string) {
    AV_SCOPE("glGetNamedProgramStringEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedProgramStringEXT(*av_t, program, target, pname, string);
}

void APIENTRY Wrap_glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glNamedRenderbufferStorageEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedRenderbufferStorageEXT(*av_t, renderbuffer, internalformat, width, height);
}

void APIENTRY Wrap_glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedRenderbufferParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedRenderbufferParameterivEXT(*av_t, renderbuffer, pname, params);
}

void APIENTRY Wrap_glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glNamedRenderbufferStorageMultisampleEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedRenderbufferStorageMultisampleEXT(*av_t, renderbuffer, samples, internalformat, width, height);
}

void APIENTRY Wrap_glNamedRenderbufferStorageMultisampleCoverageEXT(GLuint renderbuffer, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glNamedRenderbufferStorageMultisampleCoverageEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedRenderbufferStorageMultisampleCoverageEXT(*av_t, renderbuffer, coverageSamples, colorSamples, internalformat, width, height);
}

GLenum APIENTRY Wrap_glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target) {
    AV_SCOPE("glCheckNamedFramebufferStatusEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return 0;
    return ::av::glproto::av_glCheckNamedFramebufferStatusEXT(*av_t, framebuffer, target);
}

void APIENTRY Wrap_glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    AV_SCOPE("glNamedFramebufferTexture1DEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTexture1DEXT(*av_t, framebuffer, attachment, textarget, texture, level);
}

void APIENTRY Wrap_glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    AV_SCOPE("glNamedFramebufferTexture2DEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTexture2DEXT(*av_t, framebuffer, attachment, textarget, texture, level);
}

void APIENTRY Wrap_glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    AV_SCOPE("glNamedFramebufferTexture3DEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTexture3DEXT(*av_t, framebuffer, attachment, textarget, texture, level, zoffset);
}

void APIENTRY Wrap_glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    AV_SCOPE("glNamedFramebufferRenderbufferEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferRenderbufferEXT(*av_t, framebuffer, attachment, renderbuffertarget, renderbuffer);
}

void APIENTRY Wrap_glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedFramebufferAttachmentParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedFramebufferAttachmentParameterivEXT(*av_t, framebuffer, attachment, pname, params);
}

void APIENTRY Wrap_glGenerateTextureMipmapEXT(GLuint texture, GLenum target) {
    AV_SCOPE("glGenerateTextureMipmapEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenerateTextureMipmapEXT(*av_t, texture, target);
}

void APIENTRY Wrap_glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target) {
    AV_SCOPE("glGenerateMultiTexMipmapEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGenerateMultiTexMipmapEXT(*av_t, texunit, target);
}

void APIENTRY Wrap_glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum mode) {
    AV_SCOPE("glFramebufferDrawBufferEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferDrawBufferEXT(*av_t, framebuffer, mode);
}

void APIENTRY Wrap_glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
    AV_SCOPE("glFramebufferDrawBuffersEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferDrawBuffersEXT(*av_t, framebuffer, n, bufs);
}

void APIENTRY Wrap_glFramebufferReadBufferEXT(GLuint framebuffer, GLenum mode) {
    AV_SCOPE("glFramebufferReadBufferEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glFramebufferReadBufferEXT(*av_t, framebuffer, mode);
}

void APIENTRY Wrap_glGetFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint* params) {
    AV_SCOPE("glGetFramebufferParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetFramebufferParameterivEXT(*av_t, framebuffer, pname, params);
}

void APIENTRY Wrap_glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    AV_SCOPE("glNamedCopyBufferSubDataEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glicd::host_writes_buffer(writeBuffer);
    ::av::glproto::av_glNamedCopyBufferSubDataEXT(*av_t, readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void APIENTRY Wrap_glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    AV_SCOPE("glNamedFramebufferTextureEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTextureEXT(*av_t, framebuffer, attachment, texture, level);
}

void APIENTRY Wrap_glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    AV_SCOPE("glNamedFramebufferTextureLayerEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTextureLayerEXT(*av_t, framebuffer, attachment, texture, level, layer);
}

void APIENTRY Wrap_glNamedFramebufferTextureFaceEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLenum face) {
    AV_SCOPE("glNamedFramebufferTextureFaceEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferTextureFaceEXT(*av_t, framebuffer, attachment, texture, level, face);
}

void APIENTRY Wrap_glTextureRenderbufferEXT(GLuint texture, GLenum target, GLuint renderbuffer) {
    AV_SCOPE("glTextureRenderbufferEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureRenderbufferEXT(*av_t, texture, target, renderbuffer);
}

void APIENTRY Wrap_glMultiTexRenderbufferEXT(GLenum texunit, GLenum target, GLuint renderbuffer) {
    AV_SCOPE("glMultiTexRenderbufferEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glMultiTexRenderbufferEXT(*av_t, texunit, target, renderbuffer);
}

void APIENTRY Wrap_glVertexArrayVertexOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayVertexOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexOffsetEXT(*av_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayColorOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayColorOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayColorOffsetEXT(*av_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayEdgeFlagOffsetEXT(GLuint vaobj, GLuint buffer, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayEdgeFlagOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayEdgeFlagOffsetEXT(*av_t, vaobj, buffer, stride, offset);
}

void APIENTRY Wrap_glVertexArrayIndexOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayIndexOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayIndexOffsetEXT(*av_t, vaobj, buffer, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayNormalOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayNormalOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayNormalOffsetEXT(*av_t, vaobj, buffer, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayTexCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayTexCoordOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayTexCoordOffsetEXT(*av_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayMultiTexCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLenum texunit, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayMultiTexCoordOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayMultiTexCoordOffsetEXT(*av_t, vaobj, buffer, texunit, size, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayFogCoordOffsetEXT(GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayFogCoordOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayFogCoordOffsetEXT(*av_t, vaobj, buffer, type, stride, offset);
}

void APIENTRY Wrap_glVertexArraySecondaryColorOffsetEXT(GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArraySecondaryColorOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArraySecondaryColorOffsetEXT(*av_t, vaobj, buffer, size, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayVertexAttribOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribOffsetEXT(*av_t, vaobj, buffer, index, size, type, normalized, stride, offset);
}

void APIENTRY Wrap_glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayVertexAttribIOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribIOffsetEXT(*av_t, vaobj, buffer, index, size, type, stride, offset);
}

void APIENTRY Wrap_glEnableVertexArrayEXT(GLuint vaobj, GLenum array) {
    AV_SCOPE("glEnableVertexArrayEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnableVertexArrayEXT(*av_t, vaobj, array);
}

void APIENTRY Wrap_glDisableVertexArrayEXT(GLuint vaobj, GLenum array) {
    AV_SCOPE("glDisableVertexArrayEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisableVertexArrayEXT(*av_t, vaobj, array);
}

void APIENTRY Wrap_glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    AV_SCOPE("glEnableVertexArrayAttribEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glEnableVertexArrayAttribEXT(*av_t, vaobj, index);
}

void APIENTRY Wrap_glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index) {
    AV_SCOPE("glDisableVertexArrayAttribEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glDisableVertexArrayAttribEXT(*av_t, vaobj, index);
}

void APIENTRY Wrap_glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint* param) {
    AV_SCOPE("glGetVertexArrayIntegervEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexArrayIntegervEXT(*av_t, vaobj, pname, param);
}

void APIENTRY Wrap_glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, GLint* param) {
    AV_SCOPE("glGetVertexArrayIntegeri_vEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetVertexArrayIntegeri_vEXT(*av_t, vaobj, index, pname, param);
}

void APIENTRY Wrap_glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearNamedBufferDataEXT");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::clear_named_buffer(buffer, internalformat, format, type, data)) return;
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, 1, 1, 1, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glClearNamedBufferDataEXT(*av_t, buffer, internalformat, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLsizeiptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
    AV_SCOPE("glClearNamedBufferSubDataEXT");
    auto* av_t = ::av::glicd::wire();
    if (::av::glicd::clear_named_buffer_range(buffer, internalformat, offset, size, format, type, data)) return;
    if (av_t == nullptr) return;
    const bool av_data_slice = ::av::glicd::unpack_slice_bound();
    const bool av_data_offset = !av_data_slice && ::av::glicd::pixel_buffer_bound(false);
    const size_t av_data_bytes = av_data_offset ? 0 : ::av::glicd::pixel_bytes(format, type, 1, 1, 1, false);
    if (av_data_slice && !::av::glicd::slice_unpack_pixels(data, av_data_bytes)) return;
    const bool av_data_staged = !av_data_offset && ::av::glicd::stage_upload(data, av_data_bytes);
    ::av::glicd::host_writes_buffer(buffer);
    ::av::glproto::av_glClearNamedBufferSubDataEXT(*av_t, buffer, internalformat, offset, size, format, type, av_data_staged ? nullptr : data, av_data_staged ? 0 : av_data_bytes, av_data_offset || av_data_staged);
    if (av_data_staged) ::av::glicd::finish_upload();
    if (av_data_slice) ::av::glicd::unpack_slice_done();
}

void APIENTRY Wrap_glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param) {
    AV_SCOPE("glNamedFramebufferParameteriEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glNamedFramebufferParameteriEXT(*av_t, framebuffer, pname, param);
}

void APIENTRY Wrap_glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint* params) {
    AV_SCOPE("glGetNamedFramebufferParameterivEXT");
    auto* av_t = ::av::glicd::wire();
    framebuffer = ::av::glicd::framebuffer_name(framebuffer);
    if (av_t == nullptr) return;
    ::av::glproto::av_glGetNamedFramebufferParameterivEXT(*av_t, framebuffer, pname, params);
}

void APIENTRY Wrap_glProgramUniform1dEXT(GLuint program, GLint location, GLdouble x) {
    AV_SCOPE("glProgramUniform1dEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1dEXT(*av_t, program, location, x);
}

void APIENTRY Wrap_glProgramUniform2dEXT(GLuint program, GLint location, GLdouble x, GLdouble y) {
    AV_SCOPE("glProgramUniform2dEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2dEXT(*av_t, program, location, x, y);
}

void APIENTRY Wrap_glProgramUniform3dEXT(GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z) {
    AV_SCOPE("glProgramUniform3dEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3dEXT(*av_t, program, location, x, y, z);
}

void APIENTRY Wrap_glProgramUniform4dEXT(GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    AV_SCOPE("glProgramUniform4dEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4dEXT(*av_t, program, location, x, y, z, w);
}

void APIENTRY Wrap_glProgramUniform1dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform1dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform1dvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform2dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform2dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform2dvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform3dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform3dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform3dvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniform4dvEXT(GLuint program, GLint location, GLsizei count, const GLdouble* value) {
    AV_SCOPE("glProgramUniform4dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniform4dvEXT(*av_t, program, location, count, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix2dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix3dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix4dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix2x3dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x3dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix2x4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix2x4dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix2x4dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix3x2dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x2dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix3x4dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix3x4dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix3x4dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x2dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix4x2dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x2dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glProgramUniformMatrix4x3dvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value) {
    AV_SCOPE("glProgramUniformMatrix4x3dvEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glProgramUniformMatrix4x3dvEXT(*av_t, program, location, count, transpose, value);
}

void APIENTRY Wrap_glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    AV_SCOPE("glTextureBufferRangeEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureBufferRangeEXT(*av_t, texture, target, internalformat, buffer, offset, size);
}

void APIENTRY Wrap_glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    AV_SCOPE("glTextureStorage1DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage1DEXT(*av_t, texture, target, levels, internalformat, width);
}

void APIENTRY Wrap_glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    AV_SCOPE("glTextureStorage2DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage2DEXT(*av_t, texture, target, levels, internalformat, width, height);
}

void APIENTRY Wrap_glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    AV_SCOPE("glTextureStorage3DEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage3DEXT(*av_t, texture, target, levels, internalformat, width, height, depth);
}

void APIENTRY Wrap_glTextureStorage2DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTextureStorage2DMultisampleEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage2DMultisampleEXT(*av_t, texture, target, samples, internalformat, width, height, fixedsamplelocations);
}

void APIENTRY Wrap_glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    AV_SCOPE("glTextureStorage3DMultisampleEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glTextureStorage3DMultisampleEXT(*av_t, texture, target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY Wrap_glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    AV_SCOPE("glVertexArrayBindVertexBufferEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayBindVertexBufferEXT(*av_t, vaobj, bindingindex, buffer, offset, stride);
}

void APIENTRY Wrap_glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    AV_SCOPE("glVertexArrayVertexAttribFormatEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribFormatEXT(*av_t, vaobj, attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY Wrap_glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    AV_SCOPE("glVertexArrayVertexAttribIFormatEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribIFormatEXT(*av_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY Wrap_glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    AV_SCOPE("glVertexArrayVertexAttribLFormatEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribLFormatEXT(*av_t, vaobj, attribindex, size, type, relativeoffset);
}

void APIENTRY Wrap_glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    AV_SCOPE("glVertexArrayVertexAttribBindingEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribBindingEXT(*av_t, vaobj, attribindex, bindingindex);
}

void APIENTRY Wrap_glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
    AV_SCOPE("glVertexArrayVertexBindingDivisorEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexBindingDivisorEXT(*av_t, vaobj, bindingindex, divisor);
}

void APIENTRY Wrap_glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset) {
    AV_SCOPE("glVertexArrayVertexAttribLOffsetEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribLOffsetEXT(*av_t, vaobj, buffer, index, size, type, stride, offset);
}

void APIENTRY Wrap_glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor) {
    AV_SCOPE("glVertexArrayVertexAttribDivisorEXT");
    auto* av_t = ::av::glicd::wire();
    if (av_t == nullptr) return;
    ::av::glproto::av_glVertexArrayVertexAttribDivisorEXT(*av_t, vaobj, index, divisor);
}

}  // namespace av::glicd::generated
