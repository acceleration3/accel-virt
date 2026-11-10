#ifndef AV_HOST_ALLOCATION_HPP
#define AV_HOST_ALLOCATION_HPP

#include "core/support/status.hpp"
#include "guest/transport/blob_import.hpp"
#include "guest/transport/host_window_chunk.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stdint.h>

namespace av {

class GuestConnection;
class KmtTransport;
struct HostAllocationProbe;

class HostAllocation {
public:
    HostAllocation(GuestConnection &connection, KmtTransport &transport);
    ~HostAllocation();
    HostAllocation(const HostAllocation &) = delete;
    HostAllocation &operator=(const HostAllocation &) = delete;

    [[nodiscard]] Status open(uint64_t bytes, HostMemoryRequest memory, const BlobRole &role = BlobRole());
    [[nodiscard]] Status adopt(void *address, uint64_t bytes, HostMemoryRequest memory);
    [[nodiscard]] Status place(uint64_t bytes, HostMemoryRequest memory, uint64_t named);
    [[nodiscard]] Status close();
    [[nodiscard]] Status abandon();

    bool     held() const;
    bool     placed() const;
    uint64_t bytes() const;
    uint8_t *view() const;
    uint8_t *view_at(uint64_t offset, uint64_t bytes) const;
    uint64_t handle() const;
    uint32_t host_status() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status import_taken(const BlobRole &role);

    HostWindowChunk chunk_;
    BlobImport      import_;
    uint64_t        named_;
    bool            placed_;

    friend struct HostAllocationProbe;
};

}

#endif
