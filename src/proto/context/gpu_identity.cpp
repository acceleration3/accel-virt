#include "proto/context/gpu_identity.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

GpuIdentity::GpuIdentity()
    : vendor_id_(0u), device_id_(0u), device_type_(other_gpu), uuid_{}, local_memory_(0u),
      shared_memory_(0u), name_{}
      {
          AV_SCOPE("GpuIdentity::GpuIdentity");
      }

GpuIdentity::GpuIdentity(uint32_t vendor_id, uint32_t device_id, uint32_t device_type,
                         const uint8_t *uuid, uint64_t local_memory, uint64_t shared_memory,
                         const char *name)
    : vendor_id_(vendor_id), device_id_(device_id), device_type_(device_type), uuid_{},
      local_memory_(local_memory), shared_memory_(shared_memory), name_{}
{
    AV_SCOPE("GpuIdentity::GpuIdentity");
    for (size_t i = 0u; i < uuid_bytes; i++) uuid_[i] = uuid[i];
    for (size_t i = 0u; i + 1u < name_bytes && name[i] != '\0'; i++) name_[i] = name[i];
}

uint32_t       GpuIdentity::vendor_id() const
{
    AV_SCOPE("GpuIdentity::vendor_id");
    return vendor_id_;
}
uint32_t       GpuIdentity::device_id() const
{
    AV_SCOPE("GpuIdentity::device_id");
    return device_id_;
}
uint32_t       GpuIdentity::device_type() const
{
    AV_SCOPE("GpuIdentity::device_type");
    return device_type_;
}
const uint8_t *GpuIdentity::uuid() const
{
    AV_SCOPE("GpuIdentity::uuid");
    return uuid_;
}
uint64_t       GpuIdentity::local_memory() const
{
    AV_SCOPE("GpuIdentity::local_memory");
    return local_memory_;
}
uint64_t       GpuIdentity::shared_memory() const
{
    AV_SCOPE("GpuIdentity::shared_memory");
    return shared_memory_;
}
const char    *GpuIdentity::name() const
{
    AV_SCOPE("GpuIdentity::name");
    return name_;
}

void GpuIdentity::encode(WireWriter &writer) const
{
    AV_SCOPE("GpuIdentity::encode");
    writer.write_u32(vendor_id_);
    writer.write_u32(device_id_);
    writer.write_u32(device_type_);
    writer.write_zeros(4u);
    writer.copy_bytes(uuid_, uuid_bytes);
    writer.write_u64(local_memory_);
    writer.write_u64(shared_memory_);
    writer.copy_bytes(static_cast<const uint8_t *>(static_cast<const void *>(name_)), name_bytes);
}

/* NUL-terminated inside the field, and zero after the terminator, so every
 * accepted name has exactly one encoding. */
bool GpuIdentity::name_is_canonical() const
{
    AV_SCOPE("GpuIdentity::name_is_canonical");
    size_t terminator = name_bytes;
    for (size_t i = 0u; i < name_bytes; i++) {
        if (terminator == name_bytes && name_[i] == '\0') terminator = i;
        if (terminator != name_bytes && name_[i] != '\0') return false;
    }
    return terminator != name_bytes;
}

Status GpuIdentity::decode(WireReader &reader)
{
    AV_SCOPE("GpuIdentity::decode");
    GpuIdentity candidate;
    candidate.vendor_id_   = reader.read_u32();
    candidate.device_id_   = reader.read_u32();
    candidate.device_type_ = reader.read_u32();
    reader.expect_zero(4u);
    reader.copy_bytes(candidate.uuid_, uuid_bytes);
    candidate.local_memory_  = reader.read_u64();
    candidate.shared_memory_ = reader.read_u64();
    reader.copy_bytes(static_cast<uint8_t *>(static_cast<void *>(candidate.name_)), name_bytes);
    if (reader.status() != Status::ok) return reader.status();
    if (candidate.device_type_ > cpu || !candidate.name_is_canonical()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
