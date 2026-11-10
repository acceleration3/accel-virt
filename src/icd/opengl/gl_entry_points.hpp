#ifndef AV_GLICD_GL_ENTRY_POINTS_HPP
#define AV_GLICD_GL_ENTRY_POINTS_HPP

#include "gl_types.h"

#include <stdint.h>

namespace av::glicd::entry {

int32_t     APIENTRY DrvValidateVersion(uint32_t version);
void        APIENTRY DrvSetCallbackProcs(int32_t count, void *const *procs);
int32_t     APIENTRY DrvDescribePixelFormat(void *dc, int32_t index, uint32_t bytes, uint8_t *descriptor);
int32_t     APIENTRY DrvSetPixelFormat(void *dc, int32_t index);
uintptr_t   APIENTRY DrvCreateContext(void *dc);
uintptr_t   APIENTRY DrvCreateLayerContext(void *dc, int32_t layer);
int32_t     APIENTRY DrvDeleteContext(uintptr_t context);
void       *APIENTRY DrvSetContext(void *dc, uintptr_t context, void *set_proc_table);
int32_t     APIENTRY DrvReleaseContext(uintptr_t context);
int32_t     APIENTRY DrvCopyContext(uintptr_t source, uintptr_t destination, uint32_t mask);
int32_t     APIENTRY DrvShareLists(uintptr_t first, uintptr_t second);
void       *APIENTRY DrvGetProcAddress(const char *name);
int32_t     APIENTRY DrvDescribeLayerPlane(void *dc, int32_t format, int32_t plane, uint32_t bytes, void *descriptor);
int32_t     APIENTRY DrvSwapLayerBuffers(void *dc, uint32_t planes);
int32_t     APIENTRY DrvSwapBuffers(void *dc);

void       *APIENTRY wglCreateContextAttribsARB(void *dc, void *share, const int32_t *attributes);
const char *APIENTRY wglGetExtensionsStringARB(void *dc);
int32_t     APIENTRY wglGetPixelFormatAttribivARB(void *dc, int32_t format, int32_t layer, uint32_t count,
                                                  const int32_t *names, int32_t *values);
int32_t     APIENTRY wglGetPixelFormatAttribfvARB(void *dc, int32_t format, int32_t layer, uint32_t count,
                                                  const int32_t *names, float *values);
int32_t     APIENTRY wglSwapIntervalEXT(int32_t interval);
int32_t     APIENTRY wglGetSwapIntervalEXT();
int32_t     APIENTRY wglChoosePixelFormatARB(void *dc, const int32_t *ints, const float *floats, uint32_t max,
                                             int32_t *formats, uint32_t *count);

GLenum    APIENTRY get_error();
GLenum    APIENTRY get_graphics_reset_status();
int32_t   APIENTRY avForceContextFault();
uint32_t  APIENTRY avGetShareName();
uint64_t  APIENTRY avGetAdapterLuid();
int32_t   APIENTRY avRunHeldCalls();
void      APIENTRY get_integerv(GLenum pname, GLint *data);

void      APIENTRY glDeleteMemoryObjectsEXT(GLsizei n, const GLuint *memoryObjects);
void      APIENTRY glDeleteSemaphoresEXT(GLsizei n, const GLuint *semaphores);
void      APIENTRY glBufferStorageMemEXT(GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset);
void      APIENTRY glTextureStorageMem2DEXT(GLuint texture, GLsizei levels, GLenum internalFormat,
                                            GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
void      APIENTRY glSignalSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint *buffers,
                                        GLuint numTextureBarriers, const GLuint *textures,
                                        const GLenum *dstLayouts);
void      APIENTRY glWaitSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint *buffers,
                                      GLuint numTextureBarriers, const GLuint *textures,
                                      const GLenum *srcLayouts);
void      APIENTRY glNamedBufferStorageMemEXT(GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset);
void      APIENTRY glTexStorageMem1DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                                        GLuint memory, GLuint64 offset);
void      APIENTRY glTexStorageMem2DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                                        GLsizei height, GLuint memory, GLuint64 offset);
void      APIENTRY glTexStorageMem2DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat,
                                                   GLsizei width, GLsizei height,
                                                   GLboolean fixedSampleLocations, GLuint memory,
                                                   GLuint64 offset);
void      APIENTRY glTexStorageMem3DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                                        GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset);
void      APIENTRY glTexStorageMem3DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat,
                                                   GLsizei width, GLsizei height, GLsizei depth,
                                                   GLboolean fixedSampleLocations, GLuint memory,
                                                   GLuint64 offset);
void      APIENTRY glTextureStorageMem1DEXT(GLuint texture, GLsizei levels, GLenum internalFormat,
                                            GLsizei width, GLuint memory, GLuint64 offset);
void      APIENTRY glTextureStorageMem2DMultisampleEXT(GLuint texture, GLsizei samples,
                                                       GLenum internalFormat, GLsizei width, GLsizei height,
                                                       GLboolean fixedSampleLocations, GLuint memory,
                                                       GLuint64 offset);
void      APIENTRY glTextureStorageMem3DEXT(GLuint texture, GLsizei levels, GLenum internalFormat,
                                            GLsizei width, GLsizei height, GLsizei depth, GLuint memory,
                                            GLuint64 offset);
void      APIENTRY glTextureStorageMem3DMultisampleEXT(GLuint texture, GLsizei samples,
                                                       GLenum internalFormat, GLsizei width, GLsizei height,
                                                       GLsizei depth, GLboolean fixedSampleLocations,
                                                       GLuint memory, GLuint64 offset);

}

namespace av::glicd {
class DriverExports {
public:
    static constexpr uint32_t count = 15u;

    DriverExports();

    void       *address(const char *name) const;
    const char *name(uint32_t index) const;

private:
    const char *names_[count];
    void       *addresses_[count];
};

}

#endif
