#ifndef AV_DEVICE_TRACE_HPP
#define AV_DEVICE_TRACE_HPP

#include "core/support/object_storage.hpp"
#include "device/abi/av_device.h"
#include "device/shim/host_trace.hpp"
#include "device/shim/posix_condition.hpp"
#include "device/shim/posix_lock.hpp"
#include "device/shim/posix_module_resolver.hpp"
#include "device/shim/state_socket.hpp"
#include "device/shim/state_source.hpp"

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

namespace av {

class StateReporter;

using CrashHandler = void (*)(int signal, siginfo_t *info, void *context);

class DeviceTrace final : public StateSource {
public:
    static constexpr size_t   dump_bytes = 256u * 1024u;
    static constexpr uint32_t answer_seconds = 2u;

    DeviceTrace();
    DeviceTrace(const DeviceTrace &) = delete;
    DeviceTrace &operator=(const DeviceTrace &) = delete;

    void start(StateReporter &root, AvMainLoopScheduler schedule, CrashHandler handler);
    void crashed(int signal, siginfo_t *info, void *context);
    void answer_on_main_loop();
    size_t dump(char *buffer, size_t capacity, const char *&error) override;

private:
    enum class Request : uint8_t { idle, pending, answered, abandoned };
    static constexpr int      fatal[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    static constexpr uint32_t fatal_count = sizeof fatal / sizeof fatal[0];
    static constexpr size_t   path_bytes = 108u;
    static constexpr size_t   max_frames = 48u;

    void install(CrashHandler handler);
    void write_crash(int signal, uint64_t address);
    void chain(int signal, siginfo_t *info, void *context);
    bool await_dump(size_t &length);

    HostTrace            trace_;
    PosixModuleResolver  modules_;
    StateReporter       *root_;
    AvMainLoopScheduler schedule_;
    struct sigaction     previous_[fatal_count];
    uint64_t             crash_written_;
    StateSocket          socket_;
    PosixLock            lock_;
    PosixCondition       answered_;
    Request              request_;
    size_t               length_;
    char                 dump_[dump_bytes];
};

class DeviceTraceRoot {
public:
    constexpr DeviceTraceRoot() = default;
    DeviceTraceRoot(const DeviceTraceRoot &) = delete;
    DeviceTraceRoot &operator=(const DeviceTraceRoot &) = delete;

    void start(StateReporter &root, AvMainLoopScheduler schedule, CrashHandler handler);
    void crashed(int signal, siginfo_t *info, void *context);

private:
    ObjectStorage<DeviceTrace> trace_;
};

}

#endif
