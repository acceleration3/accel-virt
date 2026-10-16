/* The executor library's lines (constitution IX): one sink - QEMU's stderr,
 * made by the library's entry point - and the DriverLog each part of the
 * library writes through, every one prefixed "virtio-gpu-av: <part>: ". Made
 * once, before the root and the host's APIs, and handed to every class that
 * writes, so no class reaches for stderr and no call site types a prefix. */
#ifndef AV_EXECUTOR_LOG_HPP
#define AV_EXECUTOR_LOG_HPP

#include "core/log/driver_log.hpp"

namespace av {

class LogSink;

class ExecutorLog {
public:
    explicit ExecutorLog(LogSink &sink);
    ExecutorLog(const ExecutorLog &) = delete;
    ExecutorLog &operator=(const ExecutorLog &) = delete;

    /* A context's session, whatever its API. */
    DriverLog &sessions();
    DriverLog &vulkan();
    DriverLog &opencl();
    DriverLog &opengl();
    DriverLog &d3d9();
    DriverLog &d3d11();
    DriverLog &d3d12();
    /* Every GPU's shared objects (HostShares). */
    DriverLog &shares();

private:
    DriverLog sessions_;
    DriverLog vulkan_;
    DriverLog opencl_;
    DriverLog opengl_;
    DriverLog d3d9_;
    DriverLog d3d11_;
    DriverLog d3d12_;
    DriverLog shares_;
};

}  // namespace av

#endif  // AV_EXECUTOR_LOG_HPP
