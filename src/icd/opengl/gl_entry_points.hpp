/* The functions the ICD hands out by address (contracts/icd_interface.md): the
 * fifteen Drv* entries - which rvkgl.dll's exports forward to, and which
 * FakeOpengl32 finds by name as opengl32 finds the exports - the WGL extension
 * functions DrvGetProcAddress answers, and the recorder every GL 1.1 command
 * the core profile lacks is given in the proc table. Each is one line into the
 * root GlIcdBinding holds, and uses fixed-width types where Windows has its own
 * 32-bit ones, so one definition serves the DLL and the tests. */
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

/* Every legacy slot's entry. Win64's convention lets one function stand for
 * any signature: the caller cleans up, and the zero it returns is what a
 * legacy query such as glIsList answers. */
uintptr_t APIENTRY legacy_call();
/* glGetError: the context's own error first, then the host's. */
GLenum    APIENTRY get_error();
/* glGetIntegerv, which answers GL_NUM_EXTENSIONS itself. */
void      APIENTRY get_integerv(GLenum pname, GLint *data);

}  // namespace av::glicd::entry

namespace av::glicd {

/* The Drv* entries by name, as GetProcAddress finds exports. */
class DriverExports {
public:
    static constexpr uint32_t count = 15u;

    DriverExports();

    /* Null for a name no export has. */
    void       *address(const char *name) const;
    const char *name(uint32_t index) const;

private:
    const char *names_[count];
    void       *addresses_[count];
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_ENTRY_POINTS_HPP
