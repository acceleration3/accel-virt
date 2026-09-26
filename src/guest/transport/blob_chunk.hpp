/* One chunk a process lends the host, of whichever kind the kernel can give
 * (docs/host-window.md): a chunk of the device's host window, mapped
 * write-back, when the kernel grants one; guest RAM bound as a host-visible
 * chunk when the device has no window or the window has no room. Either way
 * it is a resource of the transport's rvk context that the device imports
 * with IMPORT_BLOB, and a view of it in this process. */
#ifndef AV_BLOB_CHUNK_HPP
#define AV_BLOB_CHUNK_HPP

#include "core/support/status.hpp"
#include "guest/transport/host_visible_chunk.hpp"
#include "guest/transport/host_window_chunk.hpp"

#include <stdint.h>

namespace av {

class GuestConnection;
class KmtTransport;
struct BlobChunkProbe;

class BlobChunk {
public:
    BlobChunk(GuestConnection &connection, KmtTransport &transport);
    BlobChunk(const BlobChunk &) = delete;
    BlobChunk &operator=(const BlobChunk &) = delete;

    /* At least `bytes`, in whole pages: the window's when the kernel grants
     * it, else guest RAM allocated, locked and bound. invalid_state when a
     * chunk is held; malformed when the kernel answered the window's open
     * with something that is not an answer, since it may then hold a chunk
     * of it; otherwise the guest RAM chunk's failure. Nothing is held unless
     * ok. */
    [[nodiscard]] Status open(uint64_t bytes);
    /* Gives back whatever is held: the first failure, and nothing is held
     * after. */
    [[nodiscard]] Status release();

    /* The resource the kernel made it, its size and this process's view of
     * its first byte; 0 and null unless held. */
    uint32_t                resource_id() const;
    uint64_t                bytes() const;
    volatile unsigned char *view() const;

    /* At most one kind held, and that one's invariants. */
    bool invariants_hold() const;

private:
    [[nodiscard]] Status open_guest_ram(uint64_t bytes);

    HostWindowChunk  window_;
    HostVisibleChunk guest_ram_;

    friend struct BlobChunkProbe;
};

}  // namespace av

#endif  // AV_BLOB_CHUNK_HPP
