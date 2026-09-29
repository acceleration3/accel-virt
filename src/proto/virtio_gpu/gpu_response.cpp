#include "proto/virtio_gpu/gpu_response.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

GpuResponse::GpuResponse() : bytes_{}, length_(0u), type_(0u)
{
    AV_SCOPE("GpuResponse::GpuResponse");
    set(ok_nodata);
}

void GpuResponse::set(uint32_t type)
{
    AV_SCOPE("GpuResponse::set");
    WireWriter writer(bytes_, sizeof bytes_);
    writer.write_u32(type);
    writer.write_zeros(sizeof bytes_ - sizeof type);
    type_ = type;
    length_ = header_size;
}

void GpuResponse::set_map_info(uint32_t map_info)
{
    AV_SCOPE("GpuResponse::set_map_info");
    set(ok_map_info);
    WireWriter writer(bytes_ + header_size, map_info_size - header_size);
    writer.write_u32(map_info);
    length_ = map_info_size;
}

void GpuResponse::set_capset_info(uint32_t capset_id, uint32_t max_version, uint32_t max_size)
{
    AV_SCOPE("GpuResponse::set_capset_info");
    set(ok_capset_info);
    WireWriter writer(bytes_ + header_size, capset_info_size - header_size);
    writer.write_u32(capset_id);
    writer.write_u32(max_version);
    writer.write_u32(max_size);
    length_ = capset_info_size;
}

void GpuResponse::set_capset(const CapsetInfo &capset)
{
    AV_SCOPE("GpuResponse::set_capset");
    set(ok_capset);
    WireWriter writer(bytes_ + header_size, CapsetInfo::encoded_size);
    capset.encode(writer);
    length_ = header_size + CapsetInfo::encoded_size;
}

void GpuResponse::set_display_modes(const DisplayModeSet &modes)
{
    AV_SCOPE("GpuResponse::set_display_modes");
    WireWriter writer(bytes_, sizeof bytes_);
    if (modes.encode(writer) != Status::ok) {
        set(err_unspec);
        return;
    }
    type_ = DisplayModeSet::response_type;
    length_ = writer.offset();
}

uint32_t GpuResponse::type() const
{
    AV_SCOPE("GpuResponse::type");
    return type_;
}
const uint8_t *GpuResponse::bytes() const
{
    AV_SCOPE("GpuResponse::bytes");
    return bytes_;
}
size_t GpuResponse::length() const
{
    AV_SCOPE("GpuResponse::length");
    return length_;
}

}  // namespace av
