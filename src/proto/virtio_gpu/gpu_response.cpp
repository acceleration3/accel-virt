#include "proto/virtio_gpu/gpu_response.hpp"

#include "proto/wire/wire_writer.hpp"

namespace av {

GpuResponse::GpuResponse() : bytes_{}, length_(0u), type_(0u) { set(ok_nodata); }

void GpuResponse::set(uint32_t type)
{
    WireWriter writer(bytes_, sizeof bytes_);
    writer.write_u32(type);
    writer.write_zeros(sizeof bytes_ - sizeof type);
    type_ = type;
    length_ = header_size;
}

void GpuResponse::set_map_info(uint32_t map_info)
{
    set(ok_map_info);
    WireWriter writer(bytes_ + header_size, map_info_size - header_size);
    writer.write_u32(map_info);
    length_ = map_info_size;
}

void GpuResponse::set_capset_info(uint32_t capset_id, uint32_t max_version, uint32_t max_size)
{
    set(ok_capset_info);
    WireWriter writer(bytes_ + header_size, capset_info_size - header_size);
    writer.write_u32(capset_id);
    writer.write_u32(max_version);
    writer.write_u32(max_size);
    length_ = capset_info_size;
}

void GpuResponse::set_capset(const CapsetInfo &capset)
{
    set(ok_capset);
    WireWriter writer(bytes_ + header_size, CapsetInfo::encoded_size);
    capset.encode(writer);
    length_ = max_length;
}

uint32_t GpuResponse::type() const { return type_; }
const uint8_t *GpuResponse::bytes() const { return bytes_; }
size_t GpuResponse::length() const { return length_; }

}  // namespace av
