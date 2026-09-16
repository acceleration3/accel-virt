#include "proto/context/context_setup.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextSetup::ContextSetup()
    : version_(0u, 0u), sub_api_(0u), ring_resource_(0u), ring_order_(0u), fence_resource_(0u), doorbell_(0u),
      reply_resource_(0u), options_()
      {
          AV_SCOPE("ContextSetup::ContextSetup");
      }

ContextSetup::ContextSetup(const ProtocolVersion &version, SubApi sub_api, uint32_t ring_resource,
                           uint8_t ring_order, uint32_t fence_resource, uint32_t doorbell,
                           uint32_t reply_resource, const GlContextOptions &options)
    : version_(version), sub_api_(static_cast<uint32_t>(sub_api)), ring_resource_(ring_resource),
      ring_order_(ring_order), fence_resource_(fence_resource), doorbell_(doorbell),
      reply_resource_(reply_resource), options_(options)
      {
          AV_SCOPE("ContextSetup::ContextSetup");
      }

const ProtocolVersion &ContextSetup::version() const
{
    AV_SCOPE("ContextSetup::version");
    return version_;
}
SubApi                 ContextSetup::sub_api() const
{
    AV_SCOPE("ContextSetup::sub_api");
    return static_cast<SubApi>(sub_api_);
}
uint32_t               ContextSetup::ring_resource() const
{
    AV_SCOPE("ContextSetup::ring_resource");
    return ring_resource_;
}
uint8_t                ContextSetup::ring_order() const
{
    AV_SCOPE("ContextSetup::ring_order");
    return ring_order_;
}
uint32_t               ContextSetup::fence_resource() const
{
    AV_SCOPE("ContextSetup::fence_resource");
    return fence_resource_;
}
uint32_t               ContextSetup::doorbell() const
{
    AV_SCOPE("ContextSetup::doorbell");
    return doorbell_;
}
uint32_t               ContextSetup::reply_resource() const
{
    AV_SCOPE("ContextSetup::reply_resource");
    return reply_resource_;
}
GlContextOptions       ContextSetup::options() const
{
    AV_SCOPE("ContextSetup::options");
    return options_;
}

size_t ContextSetup::size() const
{
    AV_SCOPE("ContextSetup::size");
    return version_.minor_version() >= options_minor ? encoded_size : base_size;
}

Status ContextSetup::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ContextSetup::encode");
    if (capacity < size()) return Status::too_large;
    if (!options_fit()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(version_.major_version());
    writer.write_u16(version_.minor_version());
    writer.write_u32(sub_api_);
    writer.write_u32(ring_resource_);
    writer.write_u8(ring_order_);
    writer.write_zeros(3u);
    writer.write_u32(fence_resource_);
    writer.write_u32(doorbell_);
    writer.write_u32(reply_resource_);
    if (size() == encoded_size) {
        options_.write(writer);
        writer.write_zeros(encoded_size - base_size - GlContextOptions::encoded_size);
    }
    return writer.status();
}

bool ContextSetup::options_fit() const
{
    AV_SCOPE("ContextSetup::options_fit");
    if (sub_api_ == static_cast<uint32_t>(SubApi::opengl) && size() == encoded_size) return options_.valid();
    return options_.defaults();
}

bool ContextSetup::fields_valid() const
{
    AV_SCOPE("ContextSetup::fields_valid");
    const bool one_known_api = sub_api_ == static_cast<uint32_t>(SubApi::vulkan) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opengl) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opencl);
    const bool distinct_reply = reply_resource_ == 0u ||
                                (reply_resource_ != ring_resource_ && reply_resource_ != fence_resource_);
    return one_known_api && ring_resource_ != 0u && fence_resource_ != 0u &&
           fence_resource_ != ring_resource_ && distinct_reply && ring_order_ >= lowest_order &&
           ring_order_ <= highest_order && options_fit();
}

Status ContextSetup::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ContextSetup::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t major = reader.read_u16();
    const uint16_t minor = reader.read_u16();
    ContextSetup candidate;
    candidate.version_       = ProtocolVersion(major, minor);
    candidate.sub_api_       = reader.read_u32();
    candidate.ring_resource_ = reader.read_u32();
    candidate.ring_order_    = reader.read_u8();
    reader.expect_zero(3u);
    candidate.fence_resource_ = reader.read_u32();
    candidate.doorbell_       = reader.read_u32();
    candidate.reply_resource_ = reader.read_u32();
    if (candidate.size() == encoded_size) {
        candidate.options_.read(reader);
        reader.expect_zero(encoded_size - base_size - GlContextOptions::encoded_size);
    }
    if (reader.status() != Status::ok) return reader.status();
    if (length != candidate.size()) return Status::malformed;
    if (found_magic != magic || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

Status ContextSetup::validate_against(const CapsetInfo &capset) const
{
    AV_SCOPE("ContextSetup::validate_against");
    if (!version_.compatible_with(capset.version()) || !capset.offers(sub_api()) ||
        !capset.accepts_ring_order(ring_order_) || doorbell_ >= capset.max_contexts())
        return Status::not_supported;
    return Status::ok;
}

}  // namespace av
