#include "icd/opengl/gl_entry_points.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"


namespace av::glicd::entry {

int32_t APIENTRY DrvValidateVersion(uint32_t version)
{
    AV_SCOPE("DrvValidateVersion");
    return gl_icd_binding.root().validate_version(version);
}

void APIENTRY DrvSetCallbackProcs(int32_t count, void *const *procs)
{
    AV_SCOPE("DrvSetCallbackProcs");
    gl_icd_binding.root().set_callback_procs(count, procs);
}

int32_t APIENTRY DrvDescribePixelFormat(void *dc, int32_t index, uint32_t bytes, uint8_t *descriptor)
{
    AV_SCOPE("DrvDescribePixelFormat");
    return gl_icd_binding.root().describe_pixel_format(dc, index, bytes, descriptor);
}

int32_t APIENTRY DrvSetPixelFormat(void *dc, int32_t index)
{
    AV_SCOPE("DrvSetPixelFormat");
    return gl_icd_binding.root().set_pixel_format(dc, index);
}

/* A handle is a 16-bit generation over a 16-bit slot (GlContextTable), so it
 * is whole in a 32-bit process's DHGLRC. */
uintptr_t APIENTRY DrvCreateContext(void *dc)
{
    AV_SCOPE("DrvCreateContext");
    return static_cast<uintptr_t>(gl_icd_binding.root().create_context(dc));
}

uintptr_t APIENTRY DrvCreateLayerContext(void *dc, int32_t layer)
{
    AV_SCOPE("DrvCreateLayerContext");
    return static_cast<uintptr_t>(gl_icd_binding.root().create_layer_context(dc, layer));
}

int32_t APIENTRY DrvDeleteContext(uintptr_t context)
{
    AV_SCOPE("DrvDeleteContext");
    return gl_icd_binding.root().delete_context(context);
}

void *APIENTRY DrvSetContext(void *dc, uintptr_t context, void *set_proc_table)
{
    AV_SCOPE("DrvSetContext");
    return gl_icd_binding.root().set_context(dc, context, set_proc_table);
}

int32_t APIENTRY DrvReleaseContext(uintptr_t context)
{
    AV_SCOPE("DrvReleaseContext");
    return gl_icd_binding.root().release_context(context);
}

int32_t APIENTRY DrvCopyContext(uintptr_t source, uintptr_t destination, uint32_t mask)
{
    AV_SCOPE("DrvCopyContext");
    return gl_icd_binding.root().copy_context(source, destination, mask);
}

int32_t APIENTRY DrvShareLists(uintptr_t first, uintptr_t second)
{
    AV_SCOPE("DrvShareLists");
    return gl_icd_binding.root().share_lists(first, second);
}

void *APIENTRY DrvGetProcAddress(const char *name)
{
    AV_SCOPE("DrvGetProcAddress");
    return gl_icd_binding.root().get_proc_address(name);
}

int32_t APIENTRY DrvDescribeLayerPlane(void *dc, int32_t format, int32_t plane, uint32_t bytes, void *descriptor)
{
    AV_SCOPE("DrvDescribeLayerPlane");
    return gl_icd_binding.root().describe_layer_plane(dc, format, plane, bytes, descriptor);
}

int32_t APIENTRY DrvSwapLayerBuffers(void *dc, uint32_t planes)
{
    AV_SCOPE("DrvSwapLayerBuffers");
    return gl_icd_binding.root().swap_layer_buffers(dc, planes);
}

int32_t APIENTRY DrvSwapBuffers(void *dc)
{
    AV_SCOPE("DrvSwapBuffers");
    return gl_icd_binding.root().swap_buffers(dc);
}

void *APIENTRY wglCreateContextAttribsARB(void *dc, void *share, const int32_t *attributes)
{
    AV_SCOPE("wglCreateContextAttribsARB");
    return gl_icd_binding.root().create_context_attribs(dc, share, attributes);
}

const char *APIENTRY wglGetExtensionsStringARB(void *dc)
{
    AV_SCOPE("wglGetExtensionsStringARB");
    return gl_icd_binding.root().extensions_string(dc);
}

int32_t APIENTRY wglGetPixelFormatAttribivARB(void *dc, int32_t format, int32_t layer, uint32_t count,
                                              const int32_t *names, int32_t *values)
{
    AV_SCOPE("wglGetPixelFormatAttribivARB");
    return gl_icd_binding.root().pixel_format_attribs(dc, format, layer, count, names, values, nullptr);
}

int32_t APIENTRY wglGetPixelFormatAttribfvARB(void *dc, int32_t format, int32_t layer, uint32_t count,
                                              const int32_t *names, float *values)
{
    AV_SCOPE("wglGetPixelFormatAttribfvARB");
    return gl_icd_binding.root().pixel_format_attribs(dc, format, layer, count, names, nullptr, values);
}

int32_t APIENTRY wglChoosePixelFormatARB(void *dc, const int32_t *ints, const float *floats, uint32_t max,
                                         int32_t *formats, uint32_t *count)
{
    AV_SCOPE("wglChoosePixelFormatARB");
    return gl_icd_binding.root().choose_pixel_format(dc, ints, floats, max, formats, count);
}

int32_t APIENTRY wglSwapIntervalEXT(int32_t interval)
{
    AV_SCOPE("wglSwapIntervalEXT");
    return gl_icd_binding.root().set_swap_interval(interval) ? 1 : 0;
}

int32_t APIENTRY wglGetSwapIntervalEXT()
{
    AV_SCOPE("wglGetSwapIntervalEXT");
    return gl_icd_binding.root().swap_interval();
}

GLenum APIENTRY get_error()
{
    AV_SCOPE("get_error");
    return gl_icd_binding.root().get_error();
}

GLenum APIENTRY get_graphics_reset_status()
{
    AV_SCOPE("get_graphics_reset_status");
    return gl_icd_binding.root().reset_status();
}

int32_t APIENTRY rvkForceContextFault()
{
    AV_SCOPE("rvkForceContextFault");
    return gl_icd_binding.root().force_context_fault();
}

uint32_t APIENTRY rvkGetShareName()
{
    AV_SCOPE("rvkGetShareName");
    return gl_icd_binding.root().current_share_name();
}

void APIENTRY get_integerv(GLenum pname, GLint *data)
{
    AV_SCOPE("get_integerv");
    gl_icd_binding.root().manual().integers(pname, data);
}


/* ---- GL_EXT_memory_object_win32 and GL_EXT_semaphore_win32 (feature 053) -- */


void APIENTRY glDeleteMemoryObjectsEXT(GLsizei n, const GLuint *memoryObjects)
{
    AV_SCOPE("glDeleteMemoryObjectsEXT");
    gl_icd_binding.root().manual().delete_memory_objects(n, memoryObjects);
}



void APIENTRY glDeleteSemaphoresEXT(GLsizei n, const GLuint *semaphores)
{
    AV_SCOPE("glDeleteSemaphoresEXT");
    gl_icd_binding.root().manual().delete_semaphores(n, semaphores);
}


void APIENTRY glBufferStorageMemEXT(GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glBufferStorageMemEXT");
    gl_icd_binding.root().manual().buffer_storage_mem(target, size, memory, offset);
}

void APIENTRY glTextureStorageMem2DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width,
                                       GLsizei height, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTextureStorageMem2DEXT");
    gl_icd_binding.root().manual().texture_storage_mem_2d(texture, levels, internalFormat, width, height,
                                                          memory, offset);
}

void APIENTRY glSignalSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint *buffers,
                                   GLuint numTextureBarriers, const GLuint *textures,
                                   const GLenum *dstLayouts)
{
    AV_SCOPE("glSignalSemaphoreEXT");
    gl_icd_binding.root().manual().signal_semaphore(semaphore, numBufferBarriers, buffers,
                                                    numTextureBarriers, textures, dstLayouts);
}

void APIENTRY glWaitSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint *buffers,
                                 GLuint numTextureBarriers, const GLuint *textures, const GLenum *srcLayouts)
{
    AV_SCOPE("glWaitSemaphoreEXT");
    gl_icd_binding.root().manual().wait_semaphore(semaphore, numBufferBarriers, buffers, numTextureBarriers,
                                                  textures, srcLayouts);
}









void APIENTRY glNamedBufferStorageMemEXT(GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glNamedBufferStorageMemEXT");
    gl_icd_binding.root().manual().named_buffer_storage_mem(buffer, size, memory, offset);
}

void APIENTRY glTexStorageMem1DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                                   GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTexStorageMem1DEXT");
    gl_icd_binding.root().manual().tex_storage_mem_1d(target, levels, internalFormat, width, memory, offset);
}

void APIENTRY glTexStorageMem2DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                                   GLsizei height, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTexStorageMem2DEXT");
    gl_icd_binding.root().manual().tex_storage_mem_2d(target, levels, internalFormat, width, height, memory,
                                                      offset);
}

void APIENTRY glTexStorageMem2DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat,
                                              GLsizei width, GLsizei height, GLboolean fixedSampleLocations,
                                              GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTexStorageMem2DMultisampleEXT");
    gl_icd_binding.root().manual().tex_storage_mem_2d_multisample(target, samples, internalFormat, width,
                                                                  height, fixedSampleLocations != 0u,
                                                                  memory, offset);
}

void APIENTRY glTexStorageMem3DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                                   GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTexStorageMem3DEXT");
    gl_icd_binding.root().manual().tex_storage_mem_3d(target, levels, internalFormat, width, height, depth,
                                                      memory, offset);
}

void APIENTRY glTexStorageMem3DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat,
                                              GLsizei width, GLsizei height, GLsizei depth,
                                              GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTexStorageMem3DMultisampleEXT");
    gl_icd_binding.root().manual().tex_storage_mem_3d_multisample(target, samples, internalFormat, width,
                                                                  height, depth,
                                                                  fixedSampleLocations != 0u, memory,
                                                                  offset);
}

void APIENTRY glTextureStorageMem1DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width,
                                       GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTextureStorageMem1DEXT");
    gl_icd_binding.root().manual().texture_storage_mem_1d(texture, levels, internalFormat, width, memory,
                                                          offset);
}

void APIENTRY glTextureStorageMem2DMultisampleEXT(GLuint texture, GLsizei samples, GLenum internalFormat,
                                                  GLsizei width, GLsizei height,
                                                  GLboolean fixedSampleLocations, GLuint memory,
                                                  GLuint64 offset)
{
    AV_SCOPE("glTextureStorageMem2DMultisampleEXT");
    gl_icd_binding.root().manual().texture_storage_mem_2d_multisample(
        texture, samples, internalFormat, width, height, fixedSampleLocations != 0u, memory, offset);
}

void APIENTRY glTextureStorageMem3DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width,
                                       GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset)
{
    AV_SCOPE("glTextureStorageMem3DEXT");
    gl_icd_binding.root().manual().texture_storage_mem_3d(texture, levels, internalFormat, width, height,
                                                          depth, memory, offset);
}

void APIENTRY glTextureStorageMem3DMultisampleEXT(GLuint texture, GLsizei samples, GLenum internalFormat,
                                                  GLsizei width, GLsizei height, GLsizei depth,
                                                  GLboolean fixedSampleLocations, GLuint memory,
                                                  GLuint64 offset)
{
    AV_SCOPE("glTextureStorageMem3DMultisampleEXT");
    gl_icd_binding.root().manual().texture_storage_mem_3d_multisample(
        texture, samples, internalFormat, width, height, depth, fixedSampleLocations != 0u, memory,
        offset);
}

}  // namespace av::glicd::entry

namespace av::glicd {

DriverExports::DriverExports()
    : names_{"DrvValidateVersion",    "DrvSetCallbackProcs", "DrvDescribePixelFormat", "DrvSetPixelFormat",
             "DrvCreateContext",      "DrvCreateLayerContext", "DrvDeleteContext",     "DrvSetContext",
             "DrvReleaseContext",     "DrvCopyContext",      "DrvShareLists",          "DrvGetProcAddress",
             "DrvDescribeLayerPlane", "DrvSwapLayerBuffers", "DrvSwapBuffers"},
      addresses_{reinterpret_cast<void *>(&entry::DrvValidateVersion),
                 reinterpret_cast<void *>(&entry::DrvSetCallbackProcs),
                 reinterpret_cast<void *>(&entry::DrvDescribePixelFormat),
                 reinterpret_cast<void *>(&entry::DrvSetPixelFormat),
                 reinterpret_cast<void *>(&entry::DrvCreateContext),
                 reinterpret_cast<void *>(&entry::DrvCreateLayerContext),
                 reinterpret_cast<void *>(&entry::DrvDeleteContext),
                 reinterpret_cast<void *>(&entry::DrvSetContext),
                 reinterpret_cast<void *>(&entry::DrvReleaseContext),
                 reinterpret_cast<void *>(&entry::DrvCopyContext),
                 reinterpret_cast<void *>(&entry::DrvShareLists),
                 reinterpret_cast<void *>(&entry::DrvGetProcAddress),
                 reinterpret_cast<void *>(&entry::DrvDescribeLayerPlane),
                 reinterpret_cast<void *>(&entry::DrvSwapLayerBuffers),
                 reinterpret_cast<void *>(&entry::DrvSwapBuffers)}
{
    AV_SCOPE("DriverExports::DriverExports");
}

void *DriverExports::address(const char *name) const
{
    AV_SCOPE("DriverExports::address");
    if (name == nullptr) return nullptr;
    for (uint32_t index = 0u; index < count; index++)
        if (__builtin_strcmp(names_[index], name) == 0) return addresses_[index];
    return nullptr;
}

const char *DriverExports::name(uint32_t index) const
{
    AV_SCOPE("DriverExports::name");
    return index < count ? names_[index] : nullptr;
}

}  // namespace av::glicd
