/* One chunk of the device's host window taken by a process
 * (docs/host-window.md): host memory the kernel placed in the window, made a
 * resource of the transport's av context and mapped into this process
 * write-back, all in one HostWindowRecord escape. There is no allocation, no
 * lock and no paging: the escape is the whole of it, and the close gives it
 * back. */
#ifndef AV_HOST_WINDOW_CHUNK_HPP
#define AV_HOST_WINDOW_CHUNK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestConnection;
class KmtTransport;
struct HostWindowChunkProbe;

class HostWindowChunk {
public:
    HostWindowChunk(GuestConnection &connection, KmtTransport &transport);
    ~HostWindowChunk();
    HostWindowChunk(const HostWindowChunk &) = delete;
    HostWindowChunk &operator=(const HostWindowChunk &) = delete;

    /* At least `bytes` of the window. invalid_state when a chunk is held and
     * invalid_parameter for no bytes, neither asked; otherwise the kernel's
     * refusal - not_supported from a device with no window - or malformed
     * when what it wrote back does not answer the open. Nothing is held
     * unless ok. */
    [[nodiscard]] Status open(uint64_t bytes);
    /* Closes what is held: the kernel's refusal, or malformed for an answer
     * that is not the close's, and nothing is held after either way. On a
     * faulted transport nothing is sent: the kernel closes the chunk with the
     * context. ok with nothing sent when nothing is held. */
    [[nodiscard]] Status release();

    /* The resource the kernel made it, and the bytes it granted; 0 unless
     * held. */
    uint32_t                resource_id() const;
    uint64_t                bytes() const;
    /* This process's write-back view of it; null unless held. */
    volatile unsigned char *view() const;

    /* A resource, a size in whole pages and a view, all three or none. */
    bool invariants_hold() const;

private:
    [[nodiscard]] Status send(uint8_t *record, uint32_t size);

    GuestConnection &connection_;
    KmtTransport    &transport_;
    uint32_t         resource_id_;
    uint64_t         bytes_;
    unsigned char   *view_;

    friend struct HostWindowChunkProbe;
};

}  // namespace av

#endif  // AV_HOST_WINDOW_CHUNK_HPP
