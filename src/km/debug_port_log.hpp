/* The driver's log on QEMU's isa-debugcon. The port costs a VM exit, and QEMU
 * a write(2), per byte - a millisecond a line - and a DPC that waited that long
 * held every processor's DPC behind its lock and pushed DWM's frame late
 * enough to drop a Windows.Graphics.Capture frame. So a line is queued under a
 * spin lock and a system thread writes it at PASSIVE_LEVEL. Before that thread
 * runs, and after it stops, a line goes to the port on the caller's thread. */
#ifndef AV_DEBUG_PORT_LOG_HPP
#define AV_DEBUG_PORT_LOG_HPP

#include <ntddk.h>

#include "core/support/log_queue.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DebugPortLog {
public:
    DebugPortLog();
    DebugPortLog(const DebugPortLog &) = delete;
    DebugPortLog &operator=(const DebugPortLog &) = delete;

    /* PASSIVE: the writer thread. */
    [[nodiscard]] Status start();
    /* PASSIVE: what is queued written out and the thread gone. */
    void                 stop();
    /* At most DISPATCH: the line, and a newline, in the order lines came. */
    void                 write(const char *line);
    /* At a bugcheck another processor may be frozen holding the lock: straight
     * to the port. */
    void                 write_now(const char *line);
    /* The writer thread's body, until stop(). */
    void                 drain();

private:
    /* QEMU isa-debugcon (docs/kb/11-implementation-plan.md, M0): one byte per
     * port write, legal at any IRQL, readable on the host from DriverEntry on. */
    static constexpr uintptr_t port = 0x402u;
    /* Seconds of the busiest logging seen, a few hundred lines. */
    static constexpr size_t    queue_bytes = 64u * 1024u;
    static constexpr size_t    chunk_bytes = 256u;

    /* Everything queued written out; false once stop() asked and none is left. */
    bool write_queued();
    void write_port(const uint8_t *bytes, size_t count);
    void report_dropped(uint64_t lines);

    /* Serializes the queue and the thread's state. Lines written with no
     * thread hold it across the port, so concurrent lines never interleave. */
    KSPIN_LOCK lock_;
    KEVENT     wake_;
    PVOID      thread_;
    bool       stopping_;
    LogQueue   queue_;
    uint8_t    storage_[queue_bytes];
};

}  // namespace av

#endif  // AV_DEBUG_PORT_LOG_HPP
