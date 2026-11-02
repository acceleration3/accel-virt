#ifndef AV_DEBUG_PORT_LOG_HPP
#define AV_DEBUG_PORT_LOG_HPP

#include <ntddk.h>

#include "core/log/log_queue.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DebugPortLog {
public:
    DebugPortLog();
    DebugPortLog(const DebugPortLog &) = delete;
    DebugPortLog &operator=(const DebugPortLog &) = delete;

    [[nodiscard]] Status start();
    void                 stop();
    void                 write(const char *line);
    void                 write_now(const char *line);
    void                 drain();

private:
    static constexpr uintptr_t port = 0x402u;
    static constexpr size_t    queue_bytes = 64u * 1024u;
    static constexpr size_t    chunk_bytes = 256u;

    bool write_queued();
    void write_port(const uint8_t *bytes, size_t count);
    void report_dropped(uint64_t lines);

    KSPIN_LOCK lock_;
    KEVENT     wake_;
    PVOID      thread_;
    bool       stopping_;
    LogQueue   queue_;
    uint8_t    storage_[queue_bytes];
};

}

#endif
