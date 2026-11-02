#include "proto/escape/umd_adapter_info.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

UmdAdapterInfo::UmdAdapterInfo() : luid_(0u), features_(0u)
{
    AV_SCOPE("UmdAdapterInfo::UmdAdapterInfo");
}

UmdAdapterInfo::UmdAdapterInfo(uint64_t luid, uint64_t features) : luid_(luid), features_(features)
{
    AV_SCOPE("UmdAdapterInfo::UmdAdapterInfo");
}

uint64_t UmdAdapterInfo::luid() const
{
    AV_SCOPE("UmdAdapterInfo::luid");
    return luid_;
}

bool UmdAdapterInfo::offers(uint64_t feature) const
{
    AV_SCOPE("UmdAdapterInfo::offers");
    return (features_ & feature) != 0u;
}

Status UmdAdapterInfo::encode(uint8_t *bytes, size_t capacity, size_t &written) const
{
    AV_SCOPE("UmdAdapterInfo::encode");
    written = 0u;
    if (capacity < base_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u64(tag);
    writer.write_u64(luid_);
    if (capacity >= encoded_size) writer.write_u64(features_);
    written = writer.offset();
    return writer.status();
}

Status UmdAdapterInfo::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("UmdAdapterInfo::decode");
    WireReader     reader(bytes, length);
    const uint64_t found_tag = reader.read_u64();
    const uint64_t luid = reader.read_u64();
    if (reader.status() != Status::ok || found_tag != tag) return Status::malformed;
    const uint64_t features = length >= encoded_size ? reader.read_u64() : 0u;
    luid_ = luid;
    features_ = features;
    return Status::ok;
}

}
