/* One host GPU's OpenCL and OpenGL side by side, as a test of GL sharing
 * needs them: the first GPU with both an OpenCL device and an EGL display, a
 * GL context open on that display - current after every call here - and an
 * OpenCL context and queue of the device's own, which names no GL context. The
 * GL objects a test shares are made, read and deleted through the helpers
 * below. A host with no such GPU is not ready, and a test returns without
 * judging, as every device test here does. */
#ifndef AV_HOST_GL_RIG_HPP
#define AV_HOST_GL_RIG_HPP

#include "executor/shim/gl_context.hpp"
#include "executor/shim/loader_host_apis.hpp"

#include <CL/cl.h>
#include <cstdint>

namespace av {

class GlDisplay;
class OpenClDevice;

class HostGlRig {
public:
    HostGlRig();
    ~HostGlRig();
    HostGlRig(const HostGlRig &) = delete;
    HostGlRig &operator=(const HostGlRig &) = delete;

    bool                ready() const;
    const OpenClDevice &device() const;
    const GlDisplay    &display() const;
    GlContext          &gl();
    cl_context          context() const;
    cl_command_queue    queue() const;

    /* A buffer of `size` bytes holding `bytes` (null: undefined). */
    std::uint32_t buffer(const void *bytes, std::uint64_t size);
    void          read_buffer(std::uint32_t buffer, void *into, std::uint64_t size);
    void          delete_buffer(std::uint32_t buffer);
    /* A texture with `level` of it defined at `target` - a cube map's face
     * defined on all six faces - as `width` x `height` x `depth` texels of
     * `internal`, the named one filled from `texels` in `format` and `type`. */
    std::uint32_t texture(std::uint32_t target, std::int32_t level, std::uint32_t internal, std::uint32_t format,
                          std::uint32_t type, std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                          const void *texels);
    void          read_texture(std::uint32_t target, std::uint32_t texture, std::int32_t level, std::uint32_t format,
                               std::uint32_t type, void *into);
    void          delete_texture(std::uint32_t texture);
    /* A `width` x `height` renderbuffer of `internal`, cleared to `rgba`. */
    std::uint32_t renderbuffer(std::uint32_t internal, std::uint32_t width, std::uint32_t height, const float rgba[4]);
    /* Its pixels as RGBA bytes. */
    void          read_renderbuffer(std::uint32_t renderbuffer, std::uint32_t width, std::uint32_t height, void *into);
    void          delete_renderbuffer(std::uint32_t renderbuffer);
    void          finish();

private:
    void *entry(const char *name);

    LoaderHostApis      host_;
    const OpenClDevice *device_;
    const GlDisplay    *display_;
    GlContext           gl_;
    cl_context          context_;
    cl_command_queue    queue_;
};

}  // namespace av

#endif  // AV_HOST_GL_RIG_HPP
