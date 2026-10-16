/* One chunk of the device's host window taken by a process
 * (docs/host-memory.md): host GPU memory of the kind the process asked for,
 * which the kernel placed in the window, made a resource of the transport's
 * av context and mapped into this process with the host's caching, all in one
 * HostWindowRecord escape - or the process's own pages lent to the device,
 * which become host GPU memory where they are ("Application memory"). There is no allocation, no
 * lock and no paging: the escape is the whole of it, and the close gives it
 * back. */
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

    /* At least `bytes` of the memory `memory` names. invalid_state when a
     * chunk is held and invalid_parameter for no bytes or no memory, neither
     * asked; otherwise the kernel's refusal - not_supported from a device
     * with no window - or malformed when what it wrote back does not answer
     * the open. Nothing is held unless ok. */
    [[nodiscard]] Status open(uint64_t bytes, HostMemoryRequest memory);
    /* The `bytes` of this process's memory at `address`, both whole pages and
     * writable, lent as host memory of the kind `memory` names: view() is
     * `address` itself from then on. As open, and invalid_parameter for a
     * range that is not whole pages. */
    [[nodiscard]] Status adopt(void *address, uint64_t bytes, HostMemoryRequest memory);
    /* Closes what is held: the kernel's refusal, or malformed for an answer
     * that is not the close's, and nothing is held after either way. On a
     * faulted transport nothing is sent: the kernel closes the chunk with the
     * context. ok with nothing sent when nothing is held. */
    [[nodiscard]] Status release();

    /* The resource the kernel made it, and the bytes it granted; 0 unless
     * held. */
    uint32_t                resource_id() const;
    uint64_t                bytes() const;
    /* This process's view of it; null unless held. */
    volatile unsigned char *view() const;

    /* A resource, a size in whole pages and a view, all three or none. */
    bool invariants_hold() const;

private:
    [[nodiscard]] Status send(uint8_t *record, uint32_t size);
    /* `record` sent and its answer taken as what is held. */
    [[nodiscard]] Status take(HostWindowRecord &record);

    GuestConnection &connection_;
    KmtTransport    &transport_;
    uint32_t         resource_id_;
    uint64_t         bytes_;
    unsigned char   *view_;

    friend struct HostWindowChunkProbe;
};

}  // namespace av

#endif  // AV_HOST_WINDOW_CHUNK_HPP
