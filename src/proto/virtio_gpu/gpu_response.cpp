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

uint32_t GpuResponse::type() const { return type_; }
const uint8_t *GpuResponse::bytes() const { return bytes_; }
size_t GpuResponse::length() const { return length_; }

}  // namespace av
