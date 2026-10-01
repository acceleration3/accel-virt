/* Before the header's ntddk.h, which it includes and must follow: waiting on a
 * thread by its handle is declared only in ntifs.h. */
#include <ntifs.h>

#include "km/debug_port_log.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

#include <intrin.h>

namespace {

/* KSTART_ROUTINE: the log is the context. */
void run_log_writer(void *log)
{
    AV_SCOPE("run_log_writer");
    static_cast<av::DebugPortLog *>(log)->drain();
}

}  // namespace

namespace av {

DebugPortLog::DebugPortLog()
    : lock_(), wake_(), thread_(nullptr), stopping_(false), queue_(storage_, sizeof storage_), storage_()
{
    AV_SCOPE("DebugPortLog::DebugPortLog");
    KeInitializeSpinLock(&lock_);
    KeInitializeEvent(&wake_, SynchronizationEvent, FALSE);
}

Status DebugPortLog::start()
{
    AV_SCOPE("DebugPortLog::start");
    HANDLE         handle = nullptr;
    const NTSTATUS created = PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr,
                                                  &run_log_writer, this);
    if (!NT_SUCCESS(created)) return Status::no_memory;
    PVOID          thread = nullptr;
    const NTSTATUS referenced = ObReferenceObjectByHandle(handle, SYNCHRONIZE, *PsThreadType, KernelMode, &thread,
                                                          nullptr);
    if (!NT_SUCCESS(referenced)) {
        /* A thread nothing can wait for would outlive the image at unload. */
        stopping_ = true;
        KeSetEvent(&wake_, IO_NO_INCREMENT, FALSE);
        static_cast<void>(ZwWaitForSingleObject(handle, FALSE, nullptr));
        ZwClose(handle);
        stopping_ = false;
        return Status::invalid_state;
    }
    ZwClose(handle);
    KIRQL previous = 0;
    KeAcquireSpinLock(&lock_, &previous);
    thread_ = thread;
    KeReleaseSpinLock(&lock_, previous);
    return Status::ok;
}

void DebugPortLog::stop()
{
    AV_SCOPE("DebugPortLog::stop");
    if (thread_ == nullptr) return;
    KIRQL previous = 0;
    KeAcquireSpinLock(&lock_, &previous);
    stopping_ = true;
    KeReleaseSpinLock(&lock_, previous);
    KeSetEvent(&wake_, IO_NO_INCREMENT, FALSE);
    static_cast<void>(KeWaitForSingleObject(thread_, Executive, KernelMode, FALSE, nullptr));
    ObDereferenceObject(thread_);
    /* A line queued after the thread's last look goes out here. */
    uint8_t chunk[chunk_bytes];
    KeAcquireSpinLock(&lock_, &previous);
    thread_ = nullptr;
    stopping_ = false;
    for (size_t taken = queue_.take(chunk, sizeof chunk); taken != 0u; taken = queue_.take(chunk, sizeof chunk))
        write_port(chunk, taken);
    KeReleaseSpinLock(&lock_, previous);
}

void DebugPortLog::write(const char *line)
{
    AV_SCOPE("DebugPortLog::write");
    KIRQL previous = 0;
    KeAcquireSpinLock(&lock_, &previous);
    const bool threaded = thread_ != nullptr;
    /* A full queue counts the line, and the thread says how many it lost. */
    if (threaded) static_cast<void>(queue_.append(line));
    else write_now(line);
    KeReleaseSpinLock(&lock_, previous);
    if (threaded) KeSetEvent(&wake_, IO_NO_INCREMENT, FALSE);
}

void DebugPortLog::write_now(const char *line)
{
    AV_SCOPE("DebugPortLog::write_now");
    size_t length = 0u;
    while (line[length] != '\0') length++;
    write_port(reinterpret_cast<const uint8_t *>(line), length);
    const uint8_t newline = '\n';
    write_port(&newline, 1u);
}

void DebugPortLog::drain()
{
    AV_SCOPE("DebugPortLog::drain");
    while (write_queued()) static_cast<void>(KeWaitForSingleObject(&wake_, Executive, KernelMode, FALSE, nullptr));
}

bool DebugPortLog::write_queued()
{
    AV_SCOPE("DebugPortLog::write_queued");
    uint8_t chunk[chunk_bytes];
    for (;;) {
        KIRQL previous = 0;
        KeAcquireSpinLock(&lock_, &previous);
        const size_t   taken = queue_.take(chunk, sizeof chunk);
        const uint64_t dropped = taken == 0u ? queue_.take_dropped() : 0u;
        const bool     stopping = stopping_;
        KeReleaseSpinLock(&lock_, previous);
        write_port(chunk, taken);
        if (dropped != 0u) report_dropped(dropped);
        if (taken == 0u) return !stopping;
    }
}

/* WRITE_PORT_BUFFER_UCHAR expands to __outbytestring, which clang-cl does not
 * provide; one byte at a time is the same port protocol. */
void DebugPortLog::write_port(const uint8_t *bytes, size_t count)
{
    AV_SCOPE("DebugPortLog::write_port");
    for (size_t index = 0u; index < count; index++)
        WRITE_PORT_UCHAR(reinterpret_cast<PUCHAR>(port), static_cast<UCHAR>(bytes[index]));
}

void DebugPortLog::report_dropped(uint64_t lines)
{
    AV_SCOPE("DebugPortLog::report_dropped");
    char       bytes[80];
    TextBuffer line(bytes, sizeof bytes);
    line.append("avgpu: ");
    line.append_decimal(lines);
    line.append(" log lines dropped: the queue was full");
    write_now(line.text());
}

}  // namespace av
