#include "harness/context/ring_blob_writer.hpp"

#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

RingBlobWriter::RingBlobWriter(BlobTable &blobs) : blobs_(blobs) {}

void RingBlobWriter::write_header(std::uint32_t id, std::uint8_t order)
{
    std::uint8_t bytes[RingHeader::encoded_size] = {};
    static_cast<void>(RingHeader(order, 0u, 0u).encode(bytes, sizeof bytes));
    if (blobs_.pin(id, 0u) != Status::ok) return;
    BlobWindow window;
    if (blobs_.window(id, window) == Status::ok) static_cast<void>(window.write(0u, bytes, sizeof bytes));
    blobs_.unpin(id);
}

}
