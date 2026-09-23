#include "proto/context/rvk_context_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RvkContextRecord::RvkContextRecord()
    : sub_api_(0u), ring_order_(0u), ring_(0u), fence_(0u), reply_(0u), options_()
    {
        AV_SCOPE("RvkContextRecord::RvkContextRecord");
    }

RvkContextRecord::RvkContextRecord(SubApi sub_api, uint8_t ring_order, uint32_t ring, uint32_t fence,
                                   uint32_t reply, const GlContextOptions &options)
    : sub_api_(static_cast<uint32_t>(sub_api)), ring_order_(ring_order), ring_(ring), fence_(fence),
      reply_(reply), options_(options)
      {
          AV_SCOPE("RvkContextRecord::RvkContextRecord");
      }

SubApi   RvkContextRecord::sub_api() const
{
    AV_SCOPE("RvkContextRecord::sub_api");
    return static_cast<SubApi>(sub_api_);
}
uint8_t  RvkContextRecord::ring_order() const
{
    AV_SCOPE("RvkContextRecord::ring_order");
    return ring_order_;
}
uint32_t RvkContextRecord::ring() const
{
    AV_SCOPE("RvkContextRecord::ring");
    return ring_;
}
uint32_t RvkContextRecord::fence() const
{
    AV_SCOPE("RvkContextRecord::fence");
    return fence_;
}
uint32_t RvkContextRecord::reply() const
{
    AV_SCOPE("RvkContextRecord::reply");
    return reply_;
}
GlContextOptions RvkContextRecord::options() const
{
    AV_SCOPE("RvkContextRecord::options");
    return options_;
}

Status RvkContextRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("RvkContextRecord::encode");
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(sub_api_);
    writer.write_u8(ring_order_);
    writer.write_zeros(3u);
    writer.write_u32(ring_);
    writer.write_u32(fence_);
    writer.write_u32(reply_);
    options_.write(writer);
    return writer.status();
}

bool RvkContextRecord::fields_valid() const
{
    AV_SCOPE("RvkContextRecord::fields_valid");
    const bool one_known_api = sub_api_ == static_cast<uint32_t>(SubApi::vulkan) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opengl) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opencl) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::d3d);
    const bool distinct_reply =
        reply_ == 0u || (reply_ != ring_ && reply_ != fence_);
    const bool options_fit =
        sub_api_ == static_cast<uint32_t>(SubApi::opengl) ? options_.valid() : options_.defaults();
    return one_known_api && ring_ != 0u && fence_ != 0u && fence_ != ring_ && distinct_reply &&
           ring_order_ >= lowest_order && ring_order_ <= highest_order && options_fit;
}

Status RvkContextRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("RvkContextRecord::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    RvkContextRecord candidate;
    candidate.sub_api_    = reader.read_u32();
    candidate.ring_order_ = reader.read_u8();
    reader.expect_zero(3u);
    candidate.ring_  = reader.read_u32();
    candidate.fence_ = reader.read_u32();
    candidate.reply_ = reader.read_u32();
    candidate.options_.read(reader);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
