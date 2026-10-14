/* One allocation of host GPU memory a process holds (docs/host-memory.md): a
 * host window chunk of the memory it asked for, mapped into the process, and
 * imported into the context it was opened on, whose answer is the handle the
 * guest names the memory by in that API's commands. Every API's mapped memory
 * - a Vulkan allocation, a Direct3D 12 heap, an API's arena - is one of these. */
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
    /* close() if still held. */
    ~HostAllocation();
    HostAllocation(const HostAllocation &) = delete;
    HostAllocation &operator=(const HostAllocation &) = delete;

    /* At least `bytes` of the memory `memory` names, opened and imported.
     * invalid_state when one is held; otherwise the failing step's status -
     * the chunk's, or the import's (BlobImport) - with nothing left held. */
    [[nodiscard]] Status open(uint64_t bytes, HostMemoryRequest memory);
    /* Releases the import, then the chunk, each attempted whatever the one
     * before answered: the first failure, and nothing is held after. ok with
     * nothing asked when nothing is held. */
    [[nodiscard]] Status close();
    /* close() for a context whose device was lost: the import is forgotten
     * rather than released - destroying the context frees it on the host -
     * and the chunk is let go. The chunk's failure, if any. */
    [[nodiscard]] Status abandon();

    bool     held() const;
    /* The bytes granted, whole pages; 0 unless held. */
    uint64_t bytes() const;
    /* The process's view; null unless held. */
    uint8_t *view() const;
    /* What the host named the import by; 0 unless held. */
    uint64_t handle() const;
    /* The status of the host's last refusal of an import (BlobImportAnswer). */
    uint32_t host_status() const;

    bool invariants_hold() const;

private:
    HostWindowChunk chunk_;
    BlobImport      import_;

    friend struct HostAllocationProbe;
};

}  // namespace av

#endif  // AV_HOST_ALLOCATION_HPP
