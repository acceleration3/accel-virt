#include "proto/virtio_gpu/gpu_response_reader.hpp"

#include "proto/wire/wire_reader.hpp"

namespace av {

GpuResponseReader::GpuResponseReader()
    : type_(0u), capset_id_(0u), capset_max_version_(0u), capset_max_size_(0u), map_info_(0u), capset_() {}

Status GpuResponseReader::decode(const uint8_t *bytes, size_t length)
{
    WireReader reader(bytes, length);
    GpuResponseReader candidate;
    candidate.type_ = reader.read_u32();
    for (size_t byte = 4u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
    if (candidate.type_ == ok_capset_info) {
        candidate.capset_id_ = reader.read_u32();
        candidate.capset_max_version_ = reader.read_u32();
        candidate.capset_max_size_ = reader.read_u32();
        static_cast<void>(reader.read_u32());   /* padding */
    }
    if (candidate.type_ == ok_map_info) {
        candidate.map_info_ = reader.read_u32();
        static_cast<void>(reader.read_u32());   /* padding */
    }
    if (reader.status() != Status::ok) return reader.status();
    if (candidate.type_ == ok_capset &&
        candidate.capset_.decode(bytes + header_size, length - header_size) != Status::ok)
        return Status::malformed;
    *this = candidate;
    return Status::ok;
}

uint32_t          GpuResponseReader::type() const               { return type_; }
bool              GpuResponseReader::ok() const                 { return type_ >= ok_range_first && type_ < error_range_first; }
uint32_t          GpuResponseReader::capset_id() const          { return capset_id_; }
uint32_t          GpuResponseReader::capset_max_version() const { return capset_max_version_; }
uint32_t          GpuResponseReader::capset_max_size() const    { return capset_max_size_; }
uint32_t          GpuResponseReader::map_info() const           { return map_info_; }
const CapsetInfo &GpuResponseReader::capset() const             { return capset_; }

}  // namespace av
