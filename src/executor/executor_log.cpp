#include "executor/executor_log.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExecutorLog::ExecutorLog(LogSink &sink)
    : sessions_(sink, "virtio-gpu-av: executor: "), vulkan_(sink, "virtio-gpu-av: vulkan: "),
      opencl_(sink, "virtio-gpu-av: opencl: "), opengl_(sink, "virtio-gpu-av: opengl: "),
      d3d9_(sink, "virtio-gpu-av: d3d9: "), d3d11_(sink, "virtio-gpu-av: d3d11: "),
      d3d12_(sink, "virtio-gpu-av: d3d12: "), shares_(sink, "virtio-gpu-av: shares: ")
{
    AV_SCOPE("ExecutorLog::ExecutorLog");
}

DriverLog &ExecutorLog::sessions()
{
    AV_SCOPE("ExecutorLog::sessions");
    return sessions_;
}

DriverLog &ExecutorLog::vulkan()
{
    AV_SCOPE("ExecutorLog::vulkan");
    return vulkan_;
}

DriverLog &ExecutorLog::opencl()
{
    AV_SCOPE("ExecutorLog::opencl");
    return opencl_;
}

DriverLog &ExecutorLog::opengl()
{
    AV_SCOPE("ExecutorLog::opengl");
    return opengl_;
}

DriverLog &ExecutorLog::d3d9()
{
    AV_SCOPE("ExecutorLog::d3d9");
    return d3d9_;
}

DriverLog &ExecutorLog::d3d11()
{
    AV_SCOPE("ExecutorLog::d3d11");
    return d3d11_;
}

DriverLog &ExecutorLog::d3d12()
{
    AV_SCOPE("ExecutorLog::d3d12");
    return d3d12_;
}

DriverLog &ExecutorLog::shares()
{
    AV_SCOPE("ExecutorLog::shares");
    return shares_;
}

}  // namespace av
