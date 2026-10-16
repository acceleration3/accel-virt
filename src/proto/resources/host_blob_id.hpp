/* The resource ids host blobs take (docs/host-memory.md): every id from
 * `first` up, above every id the driver gives a context's transport blobs, a
 * scanout or the cursor, so the device refuses any other id outright. The
 * kernel hands them out; the device only judges them. How many a guest holds
 * is the host window's to bound - each takes at least a page of it - not an
 * id count. */
#ifndef AV_HOST_BLOB_ID_HPP
#define AV_HOST_BLOB_ID_HPP

#include <stdint.h>

namespace av {

class HostBlobId {
public:
    static constexpr uint32_t first = 0x01000000u;
    /* Ids in the range: every one from `first` to the last a 32-bit resource
     * id names. */
    static constexpr uint32_t most = 0u - first;

    explicit HostBlobId(uint32_t id);

    bool valid() const;

private:
    uint32_t id_;
};

}  // namespace av

#endif  // AV_HOST_BLOB_ID_HPP
