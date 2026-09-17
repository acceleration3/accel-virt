/* rvkglprobe.exe --discovery (feature 050, T008): what the system opengl32.dll
 * does for a window this process makes - the calls an application makes first
 * (DescribePixelFormat, SetPixelFormat, wglCreateContext), each result and
 * GetLastError, which adapter the window's DC opens (D3DKMTOpenAdapterFromHdc),
 * whether rvkgl.dll is loaded afterwards, and every module the calls loaded.
 * One line per fact on standard output. Translation only: the live run is the
 * experiment, and the ICD is built on what it shows (research.md §9). */
#ifndef AV_UM_GL_DISCOVERY_PROBE_HPP
#define AV_UM_GL_DISCOVERY_PROBE_HPP

#include "core/support/text_buffer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GlDiscoveryProbe {
public:
    GlDiscoveryProbe();
    GlDiscoveryProbe(const GlDiscoveryProbe &) = delete;
    GlDiscoveryProbe &operator=(const GlDiscoveryProbe &) = delete;

    /* The exit code: 0 once every call was made and reported, whatever it
     * answered; 1 when the window could not be made. */
    uint32_t run();

private:
    static constexpr size_t   report_bytes = 16384u;
    static constexpr uint32_t max_modules = 512u;

    void     line(const char *key, const char *text, uint64_t value, uint64_t error);
    void     modules(void **list, uint32_t &count) const;
    void     report_new_modules(void *const *before, uint32_t before_count);
    bool     make_window();
    void     describe();
    void     set_format();
    void     create_context();
    void     open_adapter();
    void     write() const;

    char       bytes_[report_bytes];
    TextBuffer report_;
    void      *window_;
    void      *dc_;
    int32_t    format_;
};

}  // namespace av

#endif  // AV_UM_GL_DISCOVERY_PROBE_HPP
