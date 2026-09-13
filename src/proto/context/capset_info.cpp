#include "proto/context/capset_info.hpp"

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

CapsetInfo::CapsetInfo()
    : version_(0u, 0u), sub_apis_(0u), max_contexts_(0u), min_ring_order_(0u),
      max_ring_order_(0u), features_(0u), gpu_() {}

CapsetInfo::CapsetInfo(const ProtocolVersion &version, uint32_t sub_apis, uint32_t max_contexts,
                       uint8_t min_ring_order, uint8_t max_ring_order, uint64_t features,
                       const GpuIdentity &gpu)
    : version_(version), sub_apis_(sub_apis), max_contexts_(max_contexts),
      min_ring_order_(min_ring_order), max_ring_order_(max_ring_order), features_(features),
      gpu_(gpu) {}

const ProtocolVersion &CapsetInfo::version() const      { return version_; }
uint32_t               CapsetInfo::max_contexts() const { return max_contexts_; }
const GpuIdentity     &CapsetInfo::gpu() const          { return gpu_; }

bool CapsetInfo::offers(SubApi api) const
{
    return (sub_apis_ & static_cast<uint32_t>(api)) != 0u;
}

void CapsetInfo::withdraw(uint64_t feature) { features_ &= ~feature; }

bool CapsetInfo::accepts_ring_order(uint8_t order) const
{
    return order >= min_ring_order_ && order <= max_ring_order_;
}

bool CapsetInfo::has_feature(uint64_t feature) const { return (features_ & feature) == feature; }

Status CapsetInfo::encode(uint8_t *bytes, size_t capacity) const
{
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    encode(writer);
    return writer.status();
}

void CapsetInfo::encode(WireWriter &writer) const
{
    writer.write_u32(magic);
    writer.write_u16(version_.major_version());
    writer.write_u16(version_.minor_version());
    writer.write_u32(sub_apis_);
    writer.write_u32(max_contexts_);
    writer.write_u8(min_ring_order_);
    writer.write_u8(max_ring_order_);
    writer.write_zeros(6u);
    writer.write_u64(features_);
    gpu_.encode(writer);
}

bool CapsetInfo::fields_valid() const
{
    return sub_apis_ != 0u && (sub_apis_ & ~known_sub_apis) == 0u && max_contexts_ != 0u &&
           min_ring_order_ >= lowest_order && max_ring_order_ <= highest_order &&
           min_ring_order_ <= max_ring_order_;
}

Status CapsetInfo::decode(const uint8_t *bytes, size_t length)
{
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t major = reader.read_u16();
    const uint16_t minor = reader.read_u16();
    CapsetInfo candidate;
    candidate.version_        = ProtocolVersion(major, minor);
    candidate.sub_apis_       = reader.read_u32();
    candidate.max_contexts_   = reader.read_u32();
    candidate.min_ring_order_ = reader.read_u8();
    candidate.max_ring_order_ = reader.read_u8();
    reader.expect_zero(6u);
    candidate.features_ = reader.read_u64();
    const Status identity = candidate.gpu_.decode(reader);
    if (reader.status() != Status::ok) return reader.status();
    if (identity != Status::ok || found_magic != magic || !candidate.fields_valid())
        return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
