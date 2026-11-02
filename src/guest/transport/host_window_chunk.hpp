#ifndef AV_HOST_WINDOW_CHUNK_HPP
#define AV_HOST_WINDOW_CHUNK_HPP

#include "proto/resources/host_memory_request.hpp"

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestConnection;
class HostWindowRecord;
class KmtTransport;
struct HostWindowChunkProbe;

class HostWindowChunk {
public:
    HostWindowChunk(GuestConnection &connection, KmtTransport &transport);
    ~HostWindowChunk();
    HostWindowChunk(const HostWindowChunk &) = delete;
    HostWindowChunk &operator=(const HostWindowChunk &) = delete;

    [[nodiscard]] Status open(uint64_t bytes, HostMemoryRequest memory);
    [[nodiscard]] Status adopt(void *address, uint64_t bytes, HostMemoryRequest memory);
    [[nodiscard]] Status release();

    uint32_t                resource_id() const;
    uint64_t                bytes() const;
    volatile unsigned char *view() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status send(uint8_t *record, uint32_t size);
    [[nodiscard]] Status take(HostWindowRecord &record);

    GuestConnection &connection_;
    KmtTransport    &transport_;
    uint32_t         resource_id_;
    uint64_t         bytes_;
    unsigned char   *view_;

    friend struct HostWindowChunkProbe;
};

}

#endif
