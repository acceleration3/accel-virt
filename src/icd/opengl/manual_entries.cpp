#include "core/trace/function_scope.hpp"
#include "icd/opengl/client_vertex_state.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/manual_calls.hpp"

#include "gl_icd_entries.h"

namespace av::glicd::generated {

void APIENTRY Wrap_glArrayElement(GLint i)
{
    AV_SCOPE("Wrap_glArrayElement");
    ::av::glicd::gl_icd_binding.root().manual().array_element(i);
}

void APIENTRY Wrap_glBindBuffer(GLenum target, GLuint buffer)
{
    AV_SCOPE("Wrap_glBindBuffer");
    ::av::glicd::gl_icd_binding.root().manual().bind_buffer(target, buffer);
}

void APIENTRY Wrap_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    AV_SCOPE("Wrap_glBindFramebuffer");
    ::av::glicd::gl_icd_binding.root().manual().bind_framebuffer(target, framebuffer);
}

void APIENTRY Wrap_glBindVertexArray(GLuint array)
{
    AV_SCOPE("Wrap_glBindVertexArray");
    ::av::glicd::gl_icd_binding.root().manual().bind_vertex_array(array);
}

void APIENTRY Wrap_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    AV_SCOPE("Wrap_glBufferData");
    ::av::glicd::gl_icd_binding.root().manual().buffer_data(target, size, data, usage);
}

void APIENTRY Wrap_glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags)
{
    AV_SCOPE("Wrap_glBufferStorage");
    ::av::glicd::gl_icd_binding.root().manual().buffer_storage(target, size, data, flags);
}

void APIENTRY Wrap_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    AV_SCOPE("Wrap_glBufferSubData");
    ::av::glicd::gl_icd_binding.root().manual().buffer_sub_data(target, offset, size, data);
}

void APIENTRY Wrap_glClientActiveTexture(GLenum texture)
{
    AV_SCOPE("Wrap_glClientActiveTexture");
    ::av::glicd::gl_icd_binding.root().manual().client_active_texture(texture);
}

GLenum APIENTRY Wrap_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    AV_SCOPE("Wrap_glClientWaitSync");
    return ::av::glicd::gl_icd_binding.root().manual().client_wait_sync(sync, flags, timeout);
}

GLboolean APIENTRY Wrap_glIsFramebuffer(GLuint framebuffer)
{
    AV_SCOPE("Wrap_glIsFramebuffer");
    return ::av::glicd::gl_icd_binding.root().manual().is_framebuffer(framebuffer) ? 1 : 0;
}

void APIENTRY Wrap_glEnable(GLenum cap)
{
    AV_SCOPE("Wrap_glEnable");
    ::av::glicd::gl_icd_binding.root().manual().capability(cap, true);
}

void APIENTRY Wrap_glDisable(GLenum cap)
{
    AV_SCOPE("Wrap_glDisable");
    ::av::glicd::gl_icd_binding.root().manual().capability(cap, false);
}

GLuint64 APIENTRY Wrap_glGetTextureHandleARB(GLuint texture)
{
    AV_SCOPE("Wrap_glGetTextureHandleARB");
    return ::av::glicd::gl_icd_binding.root().manual().texture_handle(texture, 0u, false);
}

GLuint64 APIENTRY Wrap_glGetTextureSamplerHandleARB(GLuint texture, GLuint sampler)
{
    AV_SCOPE("Wrap_glGetTextureSamplerHandleARB");
    return ::av::glicd::gl_icd_binding.root().manual().texture_handle(texture, sampler, true);
}

GLuint64 APIENTRY Wrap_glGetImageHandleARB(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format)
{
    AV_SCOPE("Wrap_glGetImageHandleARB");
    return ::av::glicd::gl_icd_binding.root().manual().image_handle(texture, level, layered != 0, layer, format);
}

void APIENTRY Wrap_glMakeTextureHandleResidentARB(GLuint64 handle)
{
    AV_SCOPE("Wrap_glMakeTextureHandleResidentARB");
    ::av::glicd::gl_icd_binding.root().manual().make_handle_resident(handle, false, true, 0u);
}

void APIENTRY Wrap_glMakeTextureHandleNonResidentARB(GLuint64 handle)
{
    AV_SCOPE("Wrap_glMakeTextureHandleNonResidentARB");
    ::av::glicd::gl_icd_binding.root().manual().make_handle_resident(handle, false, false, 0u);
}

void APIENTRY Wrap_glMakeImageHandleResidentARB(GLuint64 handle, GLenum access)
{
    AV_SCOPE("Wrap_glMakeImageHandleResidentARB");
    ::av::glicd::gl_icd_binding.root().manual().make_handle_resident(handle, true, true, access);
}

void APIENTRY Wrap_glMakeImageHandleNonResidentARB(GLuint64 handle)
{
    AV_SCOPE("Wrap_glMakeImageHandleNonResidentARB");
    ::av::glicd::gl_icd_binding.root().manual().make_handle_resident(handle, true, false, 0u);
}

GLboolean APIENTRY Wrap_glIsTextureHandleResidentARB(GLuint64 handle)
{
    AV_SCOPE("Wrap_glIsTextureHandleResidentARB");
    return ::av::glicd::gl_icd_binding.root().manual().handle_resident(handle, false) ? 1 : 0;
}

GLboolean APIENTRY Wrap_glIsImageHandleResidentARB(GLuint64 handle)
{
    AV_SCOPE("Wrap_glIsImageHandleResidentARB");
    return ::av::glicd::gl_icd_binding.root().manual().handle_resident(handle, true) ? 1 : 0;
}

void APIENTRY Wrap_glUniformHandleui64ARB(GLint location, GLuint64 value)
{
    AV_SCOPE("Wrap_glUniformHandleui64ARB");
    ::av::glicd::gl_icd_binding.root().manual().uniform_handles(0u, false, location, 1, &value);
}

void APIENTRY Wrap_glUniformHandleui64vARB(GLint location, GLsizei count, const GLuint64 *value)
{
    AV_SCOPE("Wrap_glUniformHandleui64vARB");
    ::av::glicd::gl_icd_binding.root().manual().uniform_handles(0u, false, location, count, value);
}

void APIENTRY Wrap_glProgramUniformHandleui64ARB(GLuint program, GLint location, GLuint64 value)
{
    AV_SCOPE("Wrap_glProgramUniformHandleui64ARB");
    ::av::glicd::gl_icd_binding.root().manual().uniform_handles(program, true, location, 1, &value);
}

void APIENTRY Wrap_glProgramUniformHandleui64vARB(GLuint program, GLint location, GLsizei count, const GLuint64 *values)
{
    AV_SCOPE("Wrap_glProgramUniformHandleui64vARB");
    ::av::glicd::gl_icd_binding.root().manual().uniform_handles(program, true, location, count, values);
}

void APIENTRY Wrap_glDeleteTextures(GLsizei n, const GLuint *textures)
{
    AV_SCOPE("Wrap_glDeleteTextures");
    ::av::glicd::gl_icd_binding.root().manual().delete_textures(n, textures);
}

void APIENTRY Wrap_glFinish(void)
{
    AV_SCOPE("Wrap_glFinish");
    ::av::glicd::gl_icd_binding.root().finish_current();
}

void APIENTRY Wrap_glColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glColorPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::color_slot, size, type, stride, pointer);
}

void APIENTRY Wrap_glDebugMessageCallback(GLDEBUGPROC callback, const void* userParam)
{
    AV_SCOPE("Wrap_glDebugMessageCallback");
    ::av::glicd::gl_icd_binding.root().manual().debug_message_callback(reinterpret_cast<void *>(callback), userParam);
}

void APIENTRY Wrap_glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
    AV_SCOPE("Wrap_glDeleteBuffers");
    ::av::glicd::gl_icd_binding.root().manual().delete_buffers(n, buffers);
}

void APIENTRY Wrap_glDeleteProgram(GLuint program)
{
    AV_SCOPE("Wrap_glDeleteProgram");
    ::av::glicd::gl_icd_binding.root().manual().delete_program(program);
}

void APIENTRY Wrap_glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids)
{
    AV_SCOPE("Wrap_glDeleteTransformFeedbacks");
    ::av::glicd::gl_icd_binding.root().manual().delete_transform_feedbacks(n, ids);
}

void APIENTRY Wrap_glDeleteVertexArrays(GLsizei n, const GLuint* arrays)
{
    AV_SCOPE("Wrap_glDeleteVertexArrays");
    ::av::glicd::gl_icd_binding.root().manual().delete_vertex_arrays(n, arrays);
}

void APIENTRY Wrap_glDisableClientState(GLenum array)
{
    AV_SCOPE("Wrap_glDisableClientState");
    ::av::glicd::gl_icd_binding.root().manual().client_state(array, false);
}

void APIENTRY Wrap_glDisableVertexAttribArray(GLuint index)
{
    AV_SCOPE("Wrap_glDisableVertexAttribArray");
    ::av::glicd::gl_icd_binding.root().manual().enable_vertex_attrib_array(index, false);
}

void APIENTRY Wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    AV_SCOPE("Wrap_glDrawArrays");
    ::av::glicd::gl_icd_binding.root().manual().draw_arrays(mode, first, count, -1);
}

void APIENTRY Wrap_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
    AV_SCOPE("Wrap_glDrawArraysInstanced");
    ::av::glicd::gl_icd_binding.root().manual().draw_arrays(mode, first, count, instancecount);
}

void APIENTRY Wrap_glDrawBuffer(GLenum buf)
{
    AV_SCOPE("Wrap_glDrawBuffer");
    ::av::glicd::gl_icd_binding.root().manual().draw_buffer(buf);
}

void APIENTRY Wrap_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    AV_SCOPE("Wrap_glDrawElements");
    ::av::glicd::gl_icd_binding.root().manual().draw_elements(mode, count, type, indices, -1);
}

void APIENTRY Wrap_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount)
{
    AV_SCOPE("Wrap_glDrawElementsInstanced");
    ::av::glicd::gl_icd_binding.root().manual().draw_elements(mode, count, type, indices, instancecount);
}

void APIENTRY Wrap_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices)
{
    AV_SCOPE("Wrap_glDrawRangeElements");
    ::av::glicd::gl_icd_binding.root().manual().draw_range_elements(mode, start, end, count, type, indices);
}

void APIENTRY Wrap_glEdgeFlagPointer(GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glEdgeFlagPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::edge_flag_slot, 1, 0x1401u, stride, pointer);
}

void APIENTRY Wrap_glEnableClientState(GLenum array)
{
    AV_SCOPE("Wrap_glEnableClientState");
    ::av::glicd::gl_icd_binding.root().manual().client_state(array, true);
}

void APIENTRY Wrap_glEnableVertexAttribArray(GLuint index)
{
    AV_SCOPE("Wrap_glEnableVertexAttribArray");
    ::av::glicd::gl_icd_binding.root().manual().enable_vertex_attrib_array(index, true);
}

void APIENTRY Wrap_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                void *pixels)
{
    AV_SCOPE("Wrap_glReadPixels");
    ::av::glicd::gl_icd_binding.root().manual().read_pixels(x, y, width, height, format, type, pixels);
}

void APIENTRY Wrap_glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    AV_SCOPE("Wrap_glViewport");
    ::av::glicd::gl_icd_binding.root().viewport(x, y, width, height);
}

void APIENTRY Wrap_glFlush(void)
{
    AV_SCOPE("Wrap_glFlush");
    ::av::glicd::gl_icd_binding.root().flush_current();
}

void APIENTRY Wrap_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
    AV_SCOPE("Wrap_glFlushMappedBufferRange");
    ::av::glicd::gl_icd_binding.root().manual().flush_mapped_range(target, offset, length);
}

void APIENTRY Wrap_glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    AV_SCOPE("Wrap_glFlushMappedNamedBufferRange");
    ::av::glicd::gl_icd_binding.root().manual().flush_mapped_named_range(buffer, offset, length);
}

void APIENTRY Wrap_glFlushMappedNamedBufferRangeEXT(GLuint, GLintptr, GLsizeiptr)
{
    AV_SCOPE("Wrap_glFlushMappedNamedBufferRangeEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
}

void APIENTRY Wrap_glFogCoordPointer(GLenum type, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glFogCoordPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::fog_coord_slot, 1, type, stride, pointer);
}

void APIENTRY Wrap_glGenBuffers(GLsizei n, GLuint* buffers)
{
    AV_SCOPE("Wrap_glGenBuffers");
    ::av::glicd::gl_icd_binding.root().manual().gen_buffers(n, buffers);
}

void APIENTRY Wrap_glGenTextures(GLsizei n, GLuint* textures)
{
    AV_SCOPE("Wrap_glGenTextures");
    ::av::glicd::gl_icd_binding.root().manual().gen_textures(n, textures);
}

void APIENTRY Wrap_glGenVertexArrays(GLsizei n, GLuint* arrays)
{
    AV_SCOPE("Wrap_glGenVertexArrays");
    ::av::glicd::gl_icd_binding.root().manual().gen_vertex_arrays(n, arrays);
}

void APIENTRY Wrap_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data)
{
    AV_SCOPE("Wrap_glGetBufferSubData");
    ::av::glicd::gl_icd_binding.root().manual().get_buffer_sub_data(target, offset, size, data);
}

GLint APIENTRY Wrap_glGetAttribLocation(GLuint program, const GLchar* name)
{
    AV_SCOPE("Wrap_glGetAttribLocation");
    return ::av::glicd::gl_icd_binding.root().manual().attrib_location(program, name);
}

void APIENTRY Wrap_glGetBufferPointerv(GLenum target, GLenum pname, void **params)
{
    AV_SCOPE("Wrap_glGetBufferPointerv");
    ::av::glicd::gl_icd_binding.root().manual().buffer_pointer(target, pname, params);
}

void APIENTRY Wrap_glGetNamedBufferPointerv(GLuint buffer, GLenum pname, void **params)
{
    AV_SCOPE("Wrap_glGetNamedBufferPointerv");
    ::av::glicd::gl_icd_binding.root().manual().named_buffer_pointer(buffer, pname, params);
}

void APIENTRY Wrap_glGetNamedBufferPointervEXT(GLuint, GLenum, void **)
{
    AV_SCOPE("Wrap_glGetNamedBufferPointervEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
}

void APIENTRY Wrap_glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, void* data)
{
    AV_SCOPE("Wrap_glGetNamedBufferSubData");
    ::av::glicd::gl_icd_binding.root().manual().get_named_buffer_sub_data(buffer, offset, size, data);
}

void APIENTRY Wrap_glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, void* data)
{
    AV_SCOPE("Wrap_glGetNamedBufferSubDataEXT");
    ::av::glicd::gl_icd_binding.root().manual().get_named_buffer_sub_data_ext(buffer, offset, size, data);
}

void APIENTRY Wrap_glGetPointerIndexedvEXT(GLenum, GLuint, void **)
{
    AV_SCOPE("Wrap_glGetPointerIndexedvEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
}

void APIENTRY Wrap_glGetPointeri_vEXT(GLenum, GLuint, void **)
{
    AV_SCOPE("Wrap_glGetPointeri_vEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
}

void APIENTRY Wrap_glGetPointerv(GLenum pname, void **params)
{
    AV_SCOPE("Wrap_glGetPointerv");
    ::av::glicd::gl_icd_binding.root().manual().pointer(pname, params);
}

const GLubyte* APIENTRY Wrap_glGetString(GLenum name)
{
    AV_SCOPE("Wrap_glGetString");
    return ::av::glicd::gl_icd_binding.root().manual().string(name);
}

const GLubyte* APIENTRY Wrap_glGetStringi(GLenum name, GLuint index)
{
    AV_SCOPE("Wrap_glGetStringi");
    return ::av::glicd::gl_icd_binding.root().manual().string_i(name, index);
}

GLint APIENTRY Wrap_glGetUniformLocation(GLuint program, const GLchar* name)
{
    AV_SCOPE("Wrap_glGetUniformLocation");
    return ::av::glicd::gl_icd_binding.root().manual().uniform_location(program, name);
}

void APIENTRY Wrap_glGetVertexArrayPointeri_vEXT(GLuint, GLuint, GLenum, void **)
{
    AV_SCOPE("Wrap_glGetVertexArrayPointeri_vEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
}

void APIENTRY Wrap_glGetVertexArrayPointervEXT(GLuint, GLenum, void **)
{
    AV_SCOPE("Wrap_glGetVertexArrayPointervEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
}

void APIENTRY Wrap_glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
    AV_SCOPE("Wrap_glGetVertexAttribPointerv");
    ::av::glicd::gl_icd_binding.root().manual().attribute_pointer(index, pname, pointer);
}

void APIENTRY Wrap_glVertexAttribPointerARB(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glVertexAttribPointerARB");
    ::av::glicd::gl_icd_binding.root().manual().vertex_attrib_pointer(index, size, type, normalized, stride, pointer);
}

void APIENTRY Wrap_glEnableVertexAttribArrayARB(GLuint index)
{
    AV_SCOPE("Wrap_glEnableVertexAttribArrayARB");
    ::av::glicd::gl_icd_binding.root().manual().enable_vertex_attrib_array(index, true);
}

void APIENTRY Wrap_glDisableVertexAttribArrayARB(GLuint index)
{
    AV_SCOPE("Wrap_glDisableVertexAttribArrayARB");
    ::av::glicd::gl_icd_binding.root().manual().enable_vertex_attrib_array(index, false);
}

void APIENTRY Wrap_glGetProgramStringARB(GLenum target, GLenum pname, void* string)
{
    AV_SCOPE("Wrap_glGetProgramStringARB");
    ::av::glicd::gl_icd_binding.root().manual().program_string(target, pname, string);
}

void APIENTRY Wrap_glIndexPointer(GLenum type, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glIndexPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::index_slot, 1, type, stride, pointer);
}

void APIENTRY Wrap_glInterleavedArrays(GLenum format, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glInterleavedArrays");
    ::av::glicd::gl_icd_binding.root().manual().interleaved_arrays(format, stride, pointer);
}

void APIENTRY Wrap_glLinkProgram(GLuint program)
{
    AV_SCOPE("Wrap_glLinkProgram");
    ::av::glicd::gl_icd_binding.root().manual().link_program(program);
}

void* APIENTRY Wrap_glMapBuffer(GLenum target, GLenum access)
{
    AV_SCOPE("Wrap_glMapBuffer");
    return ::av::glicd::gl_icd_binding.root().manual().map_buffer(target, access);
}

void* APIENTRY Wrap_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    AV_SCOPE("Wrap_glMapBufferRange");
    return ::av::glicd::gl_icd_binding.root().manual().map_buffer_range(target, offset, length, access);
}

void* APIENTRY Wrap_glMapNamedBuffer(GLuint buffer, GLenum access)
{
    AV_SCOPE("Wrap_glMapNamedBuffer");
    return ::av::glicd::gl_icd_binding.root().manual().map_named_buffer(buffer, access);
}

void* APIENTRY Wrap_glMapNamedBufferEXT(GLuint, GLenum)
{
    AV_SCOPE("Wrap_glMapNamedBufferEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
    return nullptr;
}

void* APIENTRY Wrap_glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    AV_SCOPE("Wrap_glMapNamedBufferRange");
    return ::av::glicd::gl_icd_binding.root().manual().map_named_buffer_range(buffer, offset, length, access);
}

void* APIENTRY Wrap_glMapNamedBufferRangeEXT(GLuint, GLintptr, GLsizeiptr, GLbitfield)
{
    AV_SCOPE("Wrap_glMapNamedBufferRangeEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
    return nullptr;
}

void APIENTRY Wrap_glNamedBufferData(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage)
{
    AV_SCOPE("Wrap_glNamedBufferData");
    ::av::glicd::gl_icd_binding.root().manual().named_buffer_data(buffer, size, data, usage);
}

void APIENTRY Wrap_glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage)
{
    AV_SCOPE("Wrap_glNamedBufferDataEXT");
    ::av::glicd::gl_icd_binding.root().manual().named_buffer_data_ext(buffer, size, data, usage);
}

void APIENTRY Wrap_glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags)
{
    AV_SCOPE("Wrap_glNamedBufferStorage");
    ::av::glicd::gl_icd_binding.root().manual().named_buffer_storage(buffer, size, data, flags);
}

void APIENTRY Wrap_glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags)
{
    AV_SCOPE("Wrap_glNamedBufferStorageEXT");
    ::av::glicd::gl_icd_binding.root().manual().named_buffer_storage_ext(buffer, size, data, flags);
}

void APIENTRY Wrap_glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data)
{
    AV_SCOPE("Wrap_glNamedBufferSubData");
    ::av::glicd::gl_icd_binding.root().manual().named_buffer_sub_data(buffer, offset, size, data);
}

void APIENTRY Wrap_glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data)
{
    AV_SCOPE("Wrap_glNamedBufferSubDataEXT");
    ::av::glicd::gl_icd_binding.root().manual().named_buffer_sub_data_ext(buffer, offset, size, data);
}

void APIENTRY Wrap_glNormalPointer(GLenum type, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glNormalPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::normal_slot, 3, type, stride, pointer);
}

void APIENTRY Wrap_glPixelStoref(GLenum pname, GLfloat param)
{
    AV_SCOPE("Wrap_glPixelStoref");
    ::av::glicd::gl_icd_binding.root().manual().pixel_store(pname, param);
}

void APIENTRY Wrap_glPixelStorei(GLenum pname, GLint param)
{
    AV_SCOPE("Wrap_glPixelStorei");
    ::av::glicd::gl_icd_binding.root().manual().pixel_store(pname, param);
}

void APIENTRY Wrap_glReadBuffer(GLenum src)
{
    AV_SCOPE("Wrap_glReadBuffer");
    ::av::glicd::gl_icd_binding.root().manual().read_buffer(src);
}

void APIENTRY Wrap_glSecondaryColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glSecondaryColorPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::secondary_color_slot, size, type, stride, pointer);
}

void APIENTRY Wrap_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glTexCoordPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::texcoord_slot, size, type, stride, pointer);
}

GLboolean APIENTRY Wrap_glUnmapBuffer(GLenum target)
{
    AV_SCOPE("Wrap_glUnmapBuffer");
    return ::av::glicd::gl_icd_binding.root().manual().unmap_buffer(target);
}

GLboolean APIENTRY Wrap_glUnmapNamedBuffer(GLuint buffer)
{
    AV_SCOPE("Wrap_glUnmapNamedBuffer");
    return ::av::glicd::gl_icd_binding.root().manual().unmap_named_buffer(buffer);
}

GLboolean APIENTRY Wrap_glUnmapNamedBufferEXT(GLuint)
{
    AV_SCOPE("Wrap_glUnmapNamedBufferEXT");
    ::av::glicd::gl_icd_binding.root().manual().refuse();
    return 0;
}

void APIENTRY Wrap_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glVertexAttribPointer");
    ::av::glicd::gl_icd_binding.root().manual().vertex_attrib_pointer(index, size, type, normalized, stride, pointer);
}

void APIENTRY Wrap_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    AV_SCOPE("Wrap_glVertexPointer");
    ::av::glicd::gl_icd_binding.root().manual().array_pointer(::av::glicd::ClientVertexState::vertex_slot, size, type, stride, pointer);
}


void APIENTRY Wrap_glCreateMemoryObjectsEXT(GLsizei n, GLuint *memoryObjects)
{
    AV_SCOPE("Wrap_glCreateMemoryObjectsEXT");
    ::av::glicd::gl_icd_binding.root().manual().create_memory_objects(n, memoryObjects);
}

void APIENTRY Wrap_glGenSemaphoresEXT(GLsizei n, GLuint *semaphores)
{
    AV_SCOPE("Wrap_glGenSemaphoresEXT");
    ::av::glicd::gl_icd_binding.root().manual().gen_semaphores(n, semaphores);
}

void APIENTRY Wrap_glImportMemoryWin32HandleEXT(GLuint memory, GLuint64 size, GLenum handleType,
                                                void *handle)
{
    AV_SCOPE("Wrap_glImportMemoryWin32HandleEXT");
    ::av::glicd::gl_icd_binding.root().manual().import_memory_win32(memory, size, handleType, handle);
}

void APIENTRY Wrap_glImportSemaphoreWin32HandleEXT(GLuint semaphore, GLenum handleType, void *handle)
{
    AV_SCOPE("Wrap_glImportSemaphoreWin32HandleEXT");
    ::av::glicd::gl_icd_binding.root().manual().import_semaphore_win32(semaphore, handleType, handle);
}

void APIENTRY Wrap_glImportMemoryWin32NameEXT(GLuint memory, GLuint64 size, GLenum handleType,
                                              const void *name)
{
    AV_SCOPE("Wrap_glImportMemoryWin32NameEXT");
    ::av::glicd::gl_icd_binding.root().manual().import_memory_win32_name(memory, size, handleType, name);
}

void APIENTRY Wrap_glImportSemaphoreWin32NameEXT(GLuint semaphore, GLenum handleType, const void *name)
{
    AV_SCOPE("Wrap_glImportSemaphoreWin32NameEXT");
    ::av::glicd::gl_icd_binding.root().manual().import_semaphore_win32_name(semaphore, handleType, name);
}

GLboolean APIENTRY Wrap_glIsMemoryObjectEXT(GLuint memoryObject)
{
    AV_SCOPE("Wrap_glIsMemoryObjectEXT");
    return static_cast<GLboolean>(::av::glicd::gl_icd_binding.root().manual().is_memory_object(memoryObject));
}

GLboolean APIENTRY Wrap_glIsSemaphoreEXT(GLuint semaphore)
{
    AV_SCOPE("Wrap_glIsSemaphoreEXT");
    return static_cast<GLboolean>(::av::glicd::gl_icd_binding.root().manual().is_semaphore(semaphore));
}

void APIENTRY Wrap_glMemoryObjectParameterivEXT(GLuint memoryObject, GLenum pname, const GLint *params)
{
    AV_SCOPE("Wrap_glMemoryObjectParameterivEXT");
    ::av::glicd::gl_icd_binding.root().manual().memory_object_parameter(memoryObject, pname, params);
}

void APIENTRY Wrap_glGetMemoryObjectParameterivEXT(GLuint memoryObject, GLenum pname, GLint *params)
{
    AV_SCOPE("Wrap_glGetMemoryObjectParameterivEXT");
    ::av::glicd::gl_icd_binding.root().manual().get_memory_object_parameter(memoryObject, pname, params);
}

void APIENTRY Wrap_glSemaphoreParameterui64vEXT(GLuint semaphore, GLenum pname, const GLuint64 *params)
{
    AV_SCOPE("Wrap_glSemaphoreParameterui64vEXT");
    ::av::glicd::gl_icd_binding.root().manual().semaphore_parameter(semaphore, pname, params);
}

void APIENTRY Wrap_glGetSemaphoreParameterui64vEXT(GLuint semaphore, GLenum pname, GLuint64 *params)
{
    AV_SCOPE("Wrap_glGetSemaphoreParameterui64vEXT");
    ::av::glicd::gl_icd_binding.root().manual().get_semaphore_parameter(semaphore, pname, params);
}

}
