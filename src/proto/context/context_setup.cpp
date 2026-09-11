#include "proto/context/context_setup.hpp"

#include "proto/context/capset_info.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ContextSetup::ContextSetup()
    : version_(0u, 0u), sub_api_(0u), ring_resource_(0u), ring_order_(0u), fence_resource_(0u), doorbell_(0u) {}

ContextSetup::ContextSetup(const ProtocolVersion &version, SubApi sub_api, uint32_t ring_resource,
                           uint8_t ring_order, uint32_t fence_resource, uint32_t doorbell)
    : version_(version), sub_api_(static_cast<uint32_t>(sub_api)), ring_resource_(ring_resource),
      ring_order_(ring_order), fence_resource_(fence_resource), doorbell_(doorbell) {}

const ProtocolVersion &ContextSetup::version() const        { return version_; }
SubApi                 ContextSetup::sub_api() const        { return static_cast<SubApi>(sub_api_); }
uint32_t               ContextSetup::ring_resource() const  { return ring_resource_; }
uint8_t                ContextSetup::ring_order() const     { return ring_order_; }
uint32_t               ContextSetup::fence_resource() const { return fence_resource_; }
uint32_t               ContextSetup::doorbell() const       { return doorbell_; }

Status ContextSetup::encode(uint8_t *bytes, size_t capacity) const
{
    if (capacity < encoded_size) return Status::too_large;
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
    writer.write_zeros(4u);
    return writer.status();
}

bool ContextSetup::fields_valid() const
{
    const bool one_known_api = sub_api_ == static_cast<uint32_t>(SubApi::vulkan) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opengl) ||
                               sub_api_ == static_cast<uint32_t>(SubApi::opencl);
    return one_known_api && ring_resource_ != 0u && fence_resource_ != 0u &&
           fence_resource_ != ring_resource_ && ring_order_ >= lowest_order &&
           ring_order_ <= highest_order;
}

Status ContextSetup::decode(const uint8_t *bytes, size_t length)
{
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
    reader.expect_zero(4u);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

Status ContextSetup::validate_against(const CapsetInfo &capset) const
{
    if (!version_.compatible_with(capset.version()) || !capset.offers(sub_api()) ||
        !capset.accepts_ring_order(ring_order_) || doorbell_ >= capset.max_contexts())
        return Status::not_supported;
    return Status::ok;
}

}  // namespace av
