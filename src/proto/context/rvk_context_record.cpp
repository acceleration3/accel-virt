#include "proto/context/rvk_context_record.hpp"

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RvkContextRecord::RvkContextRecord()
    : sub_api_(0u), ring_order_(0u), ring_(0u), fence_(0u), reply_(0u) {}

RvkContextRecord::RvkContextRecord(SubApi sub_api, uint8_t ring_order, uint32_t ring, uint32_t fence,
                                   uint32_t reply)
    : sub_api_(static_cast<uint32_t>(sub_api)), ring_order_(ring_order), ring_(ring), fence_(fence),
      reply_(reply) {}

SubApi   RvkContextRecord::sub_api() const { return static_cast<SubApi>(sub_api_); }
uint8_t  RvkContextRecord::ring_order() const { return ring_order_; }
uint32_t RvkContextRecord::ring() const { return ring_; }
uint32_t RvkContextRecord::fence() const { return fence_; }
uint32_t RvkContextRecord::reply() const { return reply_; }

Status RvkContextRecord::encode(uint8_t *bytes, size_t capacity) const
{
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(sub_api_);
    writer.write_u8(ring_order_);
    writer.write_zeros(3u);
    writer.write_u32(ring_);
    writer.write_u32(fence_);
    writer.write_u32(reply_);
    writer.write_zeros(8u);
    return writer.status();
}

bool RvkContextRecord::fields_valid() const
{
    const bool one_known_api = sub_api_ == static_cast<uint32_t>(SubApi::vulkan) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opengl) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opencl);
    const bool distinct_reply =
        reply_ == 0u || (reply_ != ring_ && reply_ != fence_);
    return one_known_api && ring_ != 0u && fence_ != 0u && fence_ != ring_ && distinct_reply &&
           ring_order_ >= lowest_order && ring_order_ <= highest_order;
}

Status RvkContextRecord::decode(const uint8_t *bytes, size_t length)
{
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    RvkContextRecord candidate;
    candidate.sub_api_    = reader.read_u32();
    candidate.ring_order_ = reader.read_u8();
    reader.expect_zero(3u);
    candidate.ring_  = reader.read_u32();
    candidate.fence_ = reader.read_u32();
    candidate.reply_ = reader.read_u32();
    reader.expect_zero(8u);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
