#ifndef AV_RING_BLOB_WRITER_HPP
#define AV_RING_BLOB_WRITER_HPP

#include <cstdint>

namespace av {

class BlobTable;

class RingBlobWriter {
public:
    explicit RingBlobWriter(BlobTable &blobs);

    void write_header(std::uint32_t id, std::uint8_t order);

private:
    BlobTable &blobs_;
};

}

#endif
