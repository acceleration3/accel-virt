#include "device/ring/ring_window.hpp"

#include "proto/submission/ring_header.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RingWindow::RingWindow() : blob_(), order_(0u) {}

RingWindow::RingWindow(const BlobWindow &blob, uint8_t order) : blob_(blob), order_(order) {}

uint8_t RingWindow::order() const
{
    return order_;
}

uint64_t RingWindow::capacity() const
{
    return order_ != 0u ? uint64_t{1} << order_ : 0u;
}

Status RingWindow::snapshot(RingHeader &header) const
{
    uint8_t      bytes[RingHeader::encoded_size] = {};
    const Status status = blob_.read(0u, bytes, sizeof bytes);
    return status != Status::ok ? status : header.decode(bytes, sizeof bytes);
}

Status RingWindow::copy(uint64_t counter, uint8_t *bytes, size_t count) const
{
    if (count > capacity()) return Status::invalid_parameter;
    const uint64_t position = counter & (capacity() - 1u);
    const size_t   before_end = capacity() - position < count ? static_cast<size_t>(capacity() - position) : count;
    Status status = blob_.read(RingHeader::encoded_size + position, bytes, before_end);
    if (status == Status::ok) status = blob_.read(RingHeader::encoded_size, bytes + before_end, count - before_end);
    return status;
}

Status RingWindow::publish_head(uint64_t head) const
{
    uint8_t    bytes[sizeof head] = {};
    WireWriter writer(bytes, sizeof bytes);
    writer.write_u64(head);
    return blob_.write(head_offset, bytes, sizeof bytes);
}

bool RingWindow::invariants_hold() const
{
    return order_ >= RingHeader::lowest_order && order_ <= RingHeader::highest_order &&
           blob_.length() >= RingHeader::encoded_size + capacity();
}

}  // namespace av
