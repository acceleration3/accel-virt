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

    DriverLog &sessions();
    DriverLog &vulkan();
    DriverLog &opencl();
    DriverLog &opengl();
    DriverLog &d3d9();
    DriverLog &d3d11();
    DriverLog &d3d12();
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

}

#endif
