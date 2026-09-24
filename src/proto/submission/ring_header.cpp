#include "proto/submission/ring_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RingHeader::RingHeader() : order_(0u), tail_(0u), head_(0u)
{
    AV_SCOPE("RingHeader::RingHeader");
}

RingHeader::RingHeader(uint8_t order, uint64_t tail, uint64_t head)
    : order_(order), tail_(tail), head_(head)
    {
        AV_SCOPE("RingHeader::RingHeader");
    }

uint8_t  RingHeader::order() const
{
    AV_SCOPE("RingHeader::order");
    return order_;
}
uint64_t RingHeader::capacity() const
{
    AV_SCOPE("RingHeader::capacity");
    return uint64_t{1} << order_;
}
uint64_t RingHeader::tail() const
{
    AV_SCOPE("RingHeader::tail");
    return tail_;
}
uint64_t RingHeader::head() const
{
    AV_SCOPE("RingHeader::head");
    return head_;
}

Status RingHeader::encode(uint8_t *bytes, size_t capacity_bytes) const
{
    AV_SCOPE("RingHeader::encode");
    if (capacity_bytes < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity_bytes);
    writer.write_u32(magic);
    writer.write_u8(order_);
    writer.write_zeros(3u);
    writer.write_u64(tail_);
    writer.write_u64(head_);
    writer.write_zeros(8u);
    return writer.status();
}

Status RingHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("RingHeader::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint8_t  order = reader.read_u8();
    static_cast<void>(reader.read_u8());   /* the device's awake flag, the process's to read */
    reader.expect_zero(2u);
    const uint64_t tail = reader.read_u64();
    const uint64_t head = reader.read_u64();
    static_cast<void>(reader.read_u64());   /* the fence the guest last published, the miniport's to read */
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || order < lowest_order || order > highest_order)
        return Status::malformed;
    *this = RingHeader(order, tail, head);
    return Status::ok;
}

}  // namespace av
