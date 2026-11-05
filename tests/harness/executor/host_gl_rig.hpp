#ifndef AV_HOST_GL_RIG_HPP
#define AV_HOST_GL_RIG_HPP

#include "executor/executor_log.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/icd/recording_log.hpp"

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

    std::uint32_t buffer(const void *bytes, std::uint64_t size);
    void          read_buffer(std::uint32_t buffer, void *into, std::uint64_t size);
    void          delete_buffer(std::uint32_t buffer);
    std::uint32_t texture(std::uint32_t target, std::int32_t level, std::uint32_t internal, std::uint32_t format,
                          std::uint32_t type, std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                          const void *texels);
    void          read_texture(std::uint32_t target, std::uint32_t texture, std::int32_t level, std::uint32_t format,
                               std::uint32_t type, void *into);
    void          delete_texture(std::uint32_t texture);
    std::uint32_t renderbuffer(std::uint32_t internal, std::uint32_t width, std::uint32_t height, const float rgba[4]);
    void          read_renderbuffer(std::uint32_t renderbuffer, std::uint32_t width, std::uint32_t height, void *into);
    void          delete_renderbuffer(std::uint32_t renderbuffer);
    void          finish();

private:
    void *entry(const char *name);

    RecordingLog        sink_;
    ExecutorLog         log_;
    LoaderHostApis      host_;
    const OpenClDevice *device_;
    const GlDisplay    *display_;
    GlContext           gl_;
    cl_context          context_;
    cl_command_queue    queue_;
};

}

#endif
