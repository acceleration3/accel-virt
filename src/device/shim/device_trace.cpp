#include "device/shim/device_trace.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/crash_event.hpp"
#include "core/trace/crash_report.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_text.hpp"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <unwind.h>

namespace av {

namespace {

constexpr size_t   line_bytes = 512u;
constexpr uint64_t written = 1u;
constexpr char     main_loop_silent[] = "error the main loop did not answer within 2 s\n";
constexpr char     still_answering[] = "error the main loop has not finished the previous request\n";

struct Frames {
    uint64_t returns[64];
    uint32_t count;
};

_Unwind_Reason_Code record_frame(_Unwind_Context *context, void *argument)
{
    AV_SCOPE("record_frame");
    auto *const frames = static_cast<Frames *>(argument);
    const uint64_t address = static_cast<uint64_t>(_Unwind_GetIP(context));
    if (address == 0u || frames->count == sizeof frames->returns / sizeof frames->returns[0]) return _URC_END_OF_STACK;
    frames->returns[frames->count++] = address;
    return _URC_NO_REASON;
}

void answer_routine(void *trace)
{
    AV_SCOPE("answer_routine");
    static_cast<DeviceTrace *>(trace)->answer_on_main_loop();
}

}  // namespace

DeviceTrace::DeviceTrace()
    : trace_(), modules_(), root_(nullptr), schedule_(nullptr), previous_{},
      crash_written_(0u), socket_(), lock_(PTHREAD_MUTEX_INITIALIZER),
      answered_(PTHREAD_COND_INITIALIZER), request_(Request::idle), length_(0u), dump_{}
{
    AV_SCOPE("DeviceTrace::DeviceTrace");
}

void DeviceTrace::start(StateReporter &root, RvkMainLoopScheduler schedule, CrashHandler handler)
{
    AV_SCOPE("DeviceTrace::start");
    if (trace_.started()) return;
    if (trace_.start("device") != Status::ok) {
        static const char refused[] = "virtio-gpu-rvk: no trace table in /dev/shm; tracing is off\n";
        static_cast<void>(write(STDERR_FILENO, refused, sizeof refused - 1u));
        return;
    }
    root_ = &root;
    schedule_ = schedule;
    install(handler);
    if (schedule_ == nullptr) return;
    static_cast<void>(socket_.start("device", trace_.process(), *this));
}

void DeviceTrace::crashed(int signal, siginfo_t *info, void *context)
{
    AV_SCOPE("DeviceTrace::crashed");
    /* A machine-check SIGBUS is QEMU's to handle and the process lives on. */
    const bool machine_check = signal == SIGBUS && info != nullptr &&
                               (info->si_code == BUS_MCEERR_AR || info->si_code == BUS_MCEERR_AO);
    uint64_t   unwritten = 0u;
    if (!machine_check && trace_.started() &&
        __atomic_compare_exchange_n(&crash_written_, &unwritten, written, false, 5, 5)) {
        /* si_addr means an address only for a fault; a signal another process
         * sent carries its sender there instead. */
        const bool     fault = info != nullptr && info->si_code > 0;
        const uint64_t address = fault ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info->si_addr)) : 0u;
        trace_.table().record_crash(static_cast<uint32_t>(signal), address);
        write_crash(signal, address);
    }
    chain(signal, info, context);
}

void DeviceTrace::answer_on_main_loop()
{
    AV_SCOPE("DeviceTrace::answer_on_main_loop");
    static_cast<void>(pthread_mutex_lock(&lock_));
    const bool asked = request_ == Request::pending;
    if (!asked) request_ = Request::idle;
    static_cast<void>(pthread_mutex_unlock(&lock_));
    if (!asked) return;
    /* The socket thread does not touch the dump while a request is pending or
     * abandoned, so it is written outside the lock. */
    StateWriter writer(dump_, sizeof dump_);
    root_->report(writer);
    static_cast<void>(pthread_mutex_lock(&lock_));
    if (request_ == Request::pending) {
        request_ = Request::answered;
        length_ = writer.length();
        static_cast<void>(pthread_cond_signal(&answered_));
    } else {
        request_ = Request::idle;
    }
    static_cast<void>(pthread_mutex_unlock(&lock_));
}

void DeviceTrace::install(CrashHandler handler)
{
    AV_SCOPE("DeviceTrace::install");
    /* The handler runs on the thread's alternate stack, which the trace host
     * gives each thread the first time it asks who is calling. */
    static_cast<void>(trace_.host().thread_number());
    struct sigaction action = {};
    action.sa_sigaction = handler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    static_cast<void>(sigemptyset(&action.sa_mask));
    for (uint32_t index = 0u; index < fatal_count; index++)
        static_cast<void>(sigaction(fatal[index], &action, &previous_[index]));
}

void DeviceTrace::write_crash(int signal, uint64_t address)
{
    AV_SCOPE("DeviceTrace::write_crash");
    char       name[path_bytes];
    TextBuffer path(name, sizeof name);
    path.append("/dev/shm/crash-device-");
    path.append_decimal(trace_.process());
    path.append(".txt");
    const int file = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (file < 0) return;
    Frames frames = {};
    static_cast<void>(_Unwind_Backtrace(&record_frame, &frames));
    TraceSnapshot snapshot;
    if (snapshot.read(trace_.region(), trace_.bytes()) == Status::ok) {
        TraceText       text(snapshot, trace_.image(), trace_.host().clock());
        const CrashEvent event("device", trace_.process(), static_cast<uint32_t>(signal), address);
        CrashReport     report(event, text, modules_, frames.returns, frames.count);
        for (;;) {
            char       line[line_bytes];
            TextBuffer out(line, sizeof line);
            if (!report.next(out)) break;
            out.append("\n");
            static_cast<void>(write(file, out.text(), out.length()));
        }
    }
    static_cast<void>(close(file));
}

void DeviceTrace::chain(int signal, siginfo_t *info, void *context)
{
    AV_SCOPE("DeviceTrace::chain");
    for (uint32_t index = 0u; index < fatal_count; index++) {
        if (fatal[index] != signal) continue;
        const struct sigaction &previous = previous_[index];
        if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
            previous.sa_sigaction(signal, info, context);
        } else if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN) {
            previous.sa_handler(signal);
        } else {
            /* The action the process had before, raised again: a fault would
             * re-run on return anyway, but a signal another process sent
             * would not. It stays blocked until this handler returns. */
            static_cast<void>(sigaction(signal, &previous, nullptr));
            static_cast<void>(raise(signal));
        }
        return;
    }
}

size_t DeviceTrace::dump(char *buffer, size_t capacity, const char *&error)
{
    AV_SCOPE("DeviceTrace::dump");
    error = nullptr;
    static_cast<void>(pthread_mutex_lock(&lock_));
    const bool ready = request_ == Request::idle;
    if (ready) request_ = Request::pending;
    static_cast<void>(pthread_mutex_unlock(&lock_));
    if (!ready) {
        error = still_answering;
        return 0u;
    }
    schedule_(&answer_routine, this);
    size_t length = 0u;
    if (!await_dump(length)) {
        error = main_loop_silent;
        return 0u;
    }
    const size_t copied = length < capacity ? length : capacity;
    __builtin_memcpy(buffer, dump_, copied);
    return copied;
}

bool DeviceTrace::await_dump(size_t &length)
{
    AV_SCOPE("DeviceTrace::await_dump");
    timespec deadline = {};
    static_cast<void>(clock_gettime(CLOCK_REALTIME, &deadline));
    deadline.tv_sec += static_cast<time_t>(answer_seconds);
    static_cast<void>(pthread_mutex_lock(&lock_));
    int waited = 0;
    while (request_ == Request::pending && waited == 0) waited = pthread_cond_timedwait(&answered_, &lock_, &deadline);
    const bool answered = request_ == Request::answered;
    /* A routine still to run finds the request abandoned and only resets it. */
    request_ = answered ? Request::idle : Request::abandoned;
    length = length_;
    static_cast<void>(pthread_mutex_unlock(&lock_));
    return answered;
}

void DeviceTraceRoot::start(StateReporter &root, RvkMainLoopScheduler schedule, CrashHandler handler)
{
    AV_SCOPE("DeviceTraceRoot::start");
    if (trace_.constructed()) return;
    static_cast<void>(trace_.construct());
    trace_.object().start(root, schedule, handler);
}

void DeviceTraceRoot::crashed(int signal, siginfo_t *info, void *context)
{
    AV_SCOPE("DeviceTraceRoot::crashed");
    if (trace_.constructed()) trace_.object().crashed(signal, info, context);
}

}  // namespace av
