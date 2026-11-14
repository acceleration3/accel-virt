#include "core/trace/function_scope.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/manual_calls.hpp"

#include "gl_icd_entries.h"

namespace av::glicd::generated {

void APIENTRY Wrap_glArrayElement(GLint)
{
    AV_SCOPE("Wrap_glArrayElement");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glClientActiveTexture(GLenum)
{
    AV_SCOPE("Wrap_glClientActiveTexture");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
}

GLenum APIENTRY Wrap_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    AV_SCOPE("Wrap_glClientWaitSync");
    return ::av::glicd::gl_icd_binding.root().manual().client_wait_sync(sync, flags, timeout);
}

void APIENTRY Wrap_glFinish(void)
{
    AV_SCOPE("Wrap_glFinish");
    ::av::glicd::gl_icd_binding.root().finish_current();
}

void APIENTRY Wrap_glColorPointer(GLint, GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glColorPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glDisableClientState(GLenum)
{
    AV_SCOPE("Wrap_glDisableClientState");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glEdgeFlagPointer(GLsizei, const void*)
{
    AV_SCOPE("Wrap_glEdgeFlagPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
}

void APIENTRY Wrap_glEnableClientState(GLenum)
{
    AV_SCOPE("Wrap_glEnableClientState");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glFogCoordPointer(GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glFogCoordPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glIndexPointer(GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glIndexPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
}

void APIENTRY Wrap_glInterleavedArrays(GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glInterleavedArrays");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glNormalPointer(GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glNormalPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glSecondaryColorPointer(GLint, GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glSecondaryColorPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
}

void APIENTRY Wrap_glTexCoordPointer(GLint, GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glTexCoordPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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

void APIENTRY Wrap_glVertexPointer(GLint, GLenum, GLsizei, const void*)
{
    AV_SCOPE("Wrap_glVertexPointer");
    static_cast<void>(::av::glicd::gl_icd_binding.root().record_legacy());
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
