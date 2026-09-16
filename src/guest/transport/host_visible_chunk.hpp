/* One host-visible chunk of a process (feature 045, contracts/host_visible_memory.md
 * §1): an allocation of kind host_visible_memory on the connection's device,
 * resident through its paging queue and locked for the process's view, bound
 * by escape to the transport's rvk context as a resource the host can import.
 * The kernel's answer to the bind is read back from the record it wrote. */
#ifndef AV_HOST_VISIBLE_CHUNK_HPP
#define AV_HOST_VISIBLE_CHUNK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestConnection;
class KmtTransport;
struct HostVisibleChunkProbe;

class HostVisibleChunk {
public:
    HostVisibleChunk(GuestConnection &connection, KmtTransport &transport);
    ~HostVisibleChunk();
    HostVisibleChunk(const HostVisibleChunk &) = delete;
    HostVisibleChunk &operator=(const HostVisibleChunk &) = delete;

    /* Creates `bytes` of it and makes it resident, waiting for the paging
     * fence: the Kmt's first error, with what was made kept for release().
     * invalid_state when a chunk is already held. */
    [[nodiscard]] Status allocate(uint64_t bytes);
    /* invalid_state without an allocation or with a view already held. */
    [[nodiscard]] Status lock();
    /* The driver's own status; malformed when what it wrote back does not
     * answer the bind. invalid_state unless allocated and not yet bound. */
    [[nodiscard]] Status bind();
    /* Unbinds, unlocks and destroys whatever is held, each attempted whatever
     * the one before answered: the first failure, and nothing is held after.
     * On a faulted transport the unbind is left to the driver, which makes it
     * as the allocation is destroyed. */
    [[nodiscard]] Status release();

    /* The resource the kernel bound it as, and its pages; 0 until bound. */
    uint32_t resource_id() const;
    uint64_t pages() const;
    /* The process's view, through which every read is of the memory itself;
     * null until locked. */
    volatile unsigned char *view() const;

    bool invariants_hold() const;

private:
    GuestConnection &connection_;
    KmtTransport    &transport_;
    uint32_t         allocation_;
    unsigned char   *view_;
    uint32_t         resource_id_;
    uint64_t         pages_;

    friend struct HostVisibleChunkProbe;
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_CHUNK_HPP
