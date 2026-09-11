/* Writes a fresh ring header into a blob the device core holds, as a guest
 * driver does before it sends the context setup that names the blob. */
#ifndef AV_RING_BLOB_WRITER_HPP
#define AV_RING_BLOB_WRITER_HPP

#include <cstdint>

namespace av {

class BlobTable;

class RingBlobWriter {
public:
    explicit RingBlobWriter(BlobTable &blobs);

    /* Order `order`, tail and head 0, at offset 0 of blob `id`. */
    void write_header(std::uint32_t id, std::uint8_t order);

private:
    BlobTable &blobs_;
};

}  // namespace av

#endif  // AV_RING_BLOB_WRITER_HPP
