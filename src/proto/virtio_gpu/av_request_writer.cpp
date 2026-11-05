#include "proto/virtio_gpu/av_request_writer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/guest_page_list.hpp"

#include "proto/context/context_setup.hpp"
#include "proto/wire/wire_writer.hpp"
#include "proto/virtio_gpu/page_request.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "proto/virtio_gpu/display_timing_request.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"
#include "proto/virtio_gpu/shared_release_request.hpp"

namespace av {

AvRequestWriter::AvRequestWriter(uint8_t *bytes, size_t capacity)
    : bytes_(bytes), capacity_(bytes != nullptr ? capacity : 0u), length_(0u)
    {
        AV_SCOPE("AvRequestWriter::AvRequestWriter");
    }

bool AvRequestWriter::begin(WireWriter &writer, uint32_t type, uint32_t context_id, size_t size)
{
    AV_SCOPE("AvRequestWriter::begin");
    length_ = size <= capacity_ ? size : 0u;
    if (length_ == 0u) return false;
    writer.write_u32(type);
    writer.write_zeros(12u);
    writer.write_u32(context_id);
    writer.write_zeros(4u);
    return true;
}

Status AvRequestWriter::capset_info(uint32_t index)
{
    AV_SCOPE("AvRequestWriter::capset_info");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, get_capset_info, 0u, short_request_size)) return Status::too_large;
    writer.write_u32(index);
    writer.write_zeros(4u);
    return Status::ok;
}

Status AvRequestWriter::capset(uint32_t capset_id, uint32_t version)
{
    AV_SCOPE("AvRequestWriter::capset");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, get_capset, 0u, short_request_size)) return Status::too_large;
    writer.write_u32(capset_id);
    writer.write_u32(version);
    return Status::ok;
}

Status AvRequestWriter::create_context(uint32_t context_id, uint32_t capset_id, const char *name,
                                       uint32_t name_length)
{
    AV_SCOPE("AvRequestWriter::create_context");
    if (name_length > name_bytes || (name_length != 0u && name == nullptr)) return Status::invalid_parameter;
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, ctx_create, context_id, create_context_size)) return Status::too_large;
    writer.write_u32(name_length);
    writer.write_u32(capset_id);
    writer.write(name, name_length);
    writer.write_zeros(name_bytes - name_length);
    return Status::ok;
}

Status AvRequestWriter::destroy_context(uint32_t context_id)
{
    AV_SCOPE("AvRequestWriter::destroy_context");
    WireWriter writer(bytes_, capacity_);
    return begin(writer, ctx_destroy, context_id, header_size) ? Status::ok : Status::too_large;
}

Status AvRequestWriter::guest_blob(uint32_t resource_id, uint64_t address, uint32_t length)
{
    AV_SCOPE("AvRequestWriter::guest_blob");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_create_blob, 0u, guest_blob_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_u32(blob_memory_guest);
    writer.write_u32(0u);
    writer.write_u32(1u);
    writer.write_u64(0u);
    writer.write_u64(length);
    writer.write_u64(address);
    writer.write_u32(length);
    writer.write_zeros(4u);
    return writer.status();
}

Status AvRequestWriter::guest_blob_pages(uint32_t resource_id, const GuestPageList &pages)
{
    AV_SCOPE("AvRequestWriter::guest_blob_pages");
    if (!pages.complete()) return Status::invalid_parameter;
    const uint32_t runs = pages.runs();
    const uint64_t size = uint64_t{pages.count()} * GuestPageList::page_bytes;
    WireWriter     writer(bytes_, capacity_);
    if (!begin(writer, resource_create_blob, 0u,
               guest_blob_fixed_size + size_t{runs} * guest_blob_entry_size))
        return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_u32(blob_memory_guest);
    writer.write_u32(0u);
    writer.write_u32(runs);
    writer.write_u64(0u);
    writer.write_u64(size);
    pages.write_runs(writer);
    return writer.status();
}

Status AvRequestWriter::host_blob(uint32_t context_id, uint32_t resource_id, HostMemoryRequest memory, uint64_t size)
{
    AV_SCOPE("AvRequestWriter::host_blob");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_create_blob, context_id, host_blob_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_u32(blob_memory_host3d);
    writer.write_u32(blob_flag_mappable);
    writer.write_u32(0u);
    writer.write_u64(memory.encoded());
    writer.write_u64(size);
    return Status::ok;
}

Status AvRequestWriter::adopted_blob(uint32_t context_id, uint32_t resource_id, HostMemoryRequest memory,
                                     uint64_t size)
{
    AV_SCOPE("AvRequestWriter::adopted_blob");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_create_blob, context_id, host_blob_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_u32(blob_memory_host3d_guest);
    writer.write_u32(0u);
    writer.write_u32(0u);
    writer.write_u64(memory.encoded());
    writer.write_u64(size);
    return Status::ok;
}

Status AvRequestWriter::map_blob(uint32_t resource_id, uint64_t offset)
{
    AV_SCOPE("AvRequestWriter::map_blob");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_map_blob, 0u, map_blob_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_zeros(4u);
    writer.write_u64(offset);
    return Status::ok;
}

Status AvRequestWriter::unmap_blob(uint32_t resource_id)
{
    AV_SCOPE("AvRequestWriter::unmap_blob");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_unmap_blob, 0u, short_request_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_zeros(4u);
    return Status::ok;
}

Status AvRequestWriter::unref(uint32_t resource_id)
{
    AV_SCOPE("AvRequestWriter::unref");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, resource_unref, 0u, short_request_size)) return Status::too_large;
    writer.write_u32(resource_id);
    writer.write_zeros(4u);
    return Status::ok;
}

Status AvRequestWriter::release_shared(uint64_t object_id)
{
    AV_SCOPE("AvRequestWriter::release_shared");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, SharedReleaseRequest::type, 0u, SharedReleaseRequest::encoded_size)) return Status::too_large;
    writer.write_u64(object_id);
    return Status::ok;
}

Status AvRequestWriter::display_modes()
{
    AV_SCOPE("AvRequestWriter::display_modes");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, DisplayModeSet::request_type, 0u, DisplayModeSet::header_size)) return Status::too_large;
    return Status::ok;
}

Status AvRequestWriter::display_clock(uint64_t address)
{
    AV_SCOPE("AvRequestWriter::display_clock");
    return page(PageRequest::display_clock, address);
}

Status AvRequestWriter::present_engine(uint64_t address)
{
    AV_SCOPE("AvRequestWriter::present_engine");
    return page(PageRequest::present_engine, address);
}

Status AvRequestWriter::window_stats(uint64_t address)
{
    AV_SCOPE("AvRequestWriter::window_stats");
    return page(PageRequest::window_stats, address);
}

Status AvRequestWriter::page(uint32_t type, uint64_t address)
{
    AV_SCOPE("AvRequestWriter::page");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, type, 0u, PageRequest::encoded_size)) return Status::too_large;
    writer.write_u64(address);
    return Status::ok;
}

Status AvRequestWriter::display_timing(uint32_t head, uint32_t refresh_millihz)
{
    AV_SCOPE("AvRequestWriter::display_timing");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, DisplayTimingRequest::type, 0u, DisplayTimingRequest::encoded_size)) return Status::too_large;
    writer.write_u32(head);
    writer.write_u32(refresh_millihz);
    return Status::ok;
}

Status AvRequestWriter::scanout_plane(const ScanoutPlaneRequest &plane)
{
    AV_SCOPE("AvRequestWriter::scanout_plane");
    length_ = ScanoutPlaneRequest::encoded_size <= capacity_ ? ScanoutPlaneRequest::encoded_size : 0u;
    if (length_ == 0u) return Status::too_large;
    WireWriter writer(bytes_, capacity_);
    return plane.encode(writer);
}

Status AvRequestWriter::setup(uint32_t context_id, const ContextSetup &setup)
{
    AV_SCOPE("AvRequestWriter::setup");
    WireWriter   writer(bytes_, capacity_);
    const size_t command = ContextSetup::encoded_size;
    if (!begin(writer, submit_3d, context_id, short_request_size + command)) return Status::too_large;
    writer.write_u32(static_cast<uint32_t>(command));
    writer.write_zeros(4u);
    const Status encoded = setup.encode(bytes_ + short_request_size, command);
    if (encoded != Status::ok) length_ = 0u;
    return encoded;
}

size_t AvRequestWriter::length() const
{
    AV_SCOPE("AvRequestWriter::length");
    return length_;
}

}
