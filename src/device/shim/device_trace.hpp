/* What the device module adds to its trace table in QEMU's process
 * (docs/trace.md): a crash file written from the fatal signals, chained to
 * whatever handled them before, and the state socket
 * /dev/shm/av-state-device-<pid>.sock, whose answer the main loop writes
 * under the big lock because that is the lock the module's devices live
 * under. A main loop that does not answer in time is reported as such, never
 * waited on. One per process, in the ABI translation unit's storage. */
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

    /* The table, the crash handler and the socket; each part that cannot start
     * is logged to standard error and left off. */
    void start(StateReporter &root, AvMainLoopScheduler schedule, CrashHandler handler);
    /* From the fatal signal's handler. */
    void crashed(int signal, siginfo_t *info, void *context);
    /* The scheduled main-loop routine: the dump a socket client waits for. */
    void answer_on_main_loop();
    /* The socket's question, handed to the main loop and waited for. */
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
    /* The dump's length once the main loop wrote it, or false after the wait. */
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

/* The storage entry_points.cpp keeps: a DeviceTrace made on the first start. */
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

}  // namespace av

#endif  // AV_DEVICE_TRACE_HPP
