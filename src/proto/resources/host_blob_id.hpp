/* The resource ids host blobs take (docs/host-memory.md): a range of their
 * own, above every id the driver gives a context's transport blobs, a
 * scanout or the cursor, so the device can find a host blob by its id
 * without searching and refuse any other id outright. The kernel hands
 * them out; the device only judges them. */
#ifndef AV_HOST_BLOB_ID_HPP
#define AV_HOST_BLOB_ID_HPP

#include <stdint.h>

namespace av {

class HostBlobId {
public:
    static constexpr uint32_t first = 0x01000000u;
    /* As many as the device keeps at once: what Vulkan reports as
     * maxMemoryAllocationCount on most drivers, for the whole guest. */
    static constexpr uint32_t count = 16384u;

    explicit HostBlobId(uint32_t id);

    bool     valid() const;
    /* Which of the `count` it is; meaningful only when valid. */
    uint32_t slot() const;

private:
    uint32_t id_;
};

}  // namespace av

#endif  // AV_HOST_BLOB_ID_HPP
