#include "miniport/memory/host_visible_blob.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

namespace av {

HostVisibleBlob::HostVisibleBlob(Platform &platform)
    : platform_(platform), resource_(0u), offset_(0u), mapped_(false), caching_(PageCaching::write_back), request_{},
      answer_{}
{
    AV_SCOPE("HostVisibleBlob::HostVisibleBlob");
}

HostVisibleBlob::HostVisibleBlob(Platform &platform, uint32_t resource, uint64_t offset)
    : platform_(platform), resource_(resource), offset_(offset), mapped_(true), caching_(PageCaching::write_back),
      request_{}, answer_{}
{
    AV_SCOPE("HostVisibleBlob::HostVisibleBlob");
}

HostVisibleBlob::HostVisibleBlob(Platform &platform, uint32_t resource)
    : platform_(platform), resource_(resource), offset_(0u), mapped_(false), caching_(PageCaching::write_back),
      request_{}, answer_{}
{
    AV_SCOPE("HostVisibleBlob::HostVisibleBlob");
}

Status HostVisibleBlob::create(VirtioGpuDevice &device, uint32_t context_id, uint32_t resource_id,
                               HostMemoryRequest memory, uint64_t bytes)
{
    AV_SCOPE("HostVisibleBlob::create");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::create");
    if (resource_ != 0u) return Status::invalid_state;
    if (context_id == 0u || resource_id == 0u || !memory.valid() || bytes == 0u || (bytes & (page_bytes - 1u)) != 0u)
        return Status::invalid_parameter;
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.host_blob(context_id, resource_id, memory, bytes));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, request_, writer.length(), map_info);
    if (status == Status::ok || status == Status::not_found) resource_ = resource_id;
    return status;
}

Status HostVisibleBlob::create_over(VirtioGpuDevice &device, uint32_t context_id, uint32_t resource_id,
                                    HostMemoryRequest memory, void *locked, uint64_t bytes)
{
    AV_SCOPE("HostVisibleBlob::create_over");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::create_over");
    if (resource_ != 0u) return Status::invalid_state;
    if (context_id == 0u || resource_id == 0u || !memory.valid() || locked == nullptr || bytes == 0u ||
        (bytes & (page_bytes - 1u)) != 0u || bytes / page_bytes > UINT32_MAX)
        return Status::invalid_parameter;
    auto *const request = static_cast<uint8_t *>(
        platform_.allocate(GpuRequestWriter::max_backing_request_size, Pool::non_paged));
    auto *const frames = static_cast<uint64_t *>(platform_.allocate(pages_per_piece * sizeof(uint64_t), Pool::paged));
    ScopeExit   release([this, request, frames] {
        if (request != nullptr) platform_.release(request);
        if (frames != nullptr) platform_.release(frames);
    });
    if (request == nullptr || frames == nullptr) return Status::no_memory;
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.adopted_blob(context_id, resource_id, memory, bytes));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, request_, writer.length(), map_info);
    if (status == Status::ok || status == Status::not_found) resource_ = resource_id;
    if (status != Status::ok) return status;
    return attach_pieces(device, locked, bytes, request, frames);
}

Status HostVisibleBlob::attach_pieces(VirtioGpuDevice &device, void *locked, uint64_t bytes, uint8_t *request,
                                      uint64_t *frames)
{
    AV_SCOPE("HostVisibleBlob::attach_pieces");
    const auto pages = static_cast<uint32_t>(bytes / page_bytes);
    for (uint32_t first = 0u; first < pages; first += pages_per_piece) {
        const uint32_t count = pages - first < pages_per_piece ? pages - first : pages_per_piece;
        const Status   read = platform_.read_page_frames(locked, first, count, frames);
        if (read != Status::ok) return read;
        GpuRequestWriter writer(request, GpuRequestWriter::max_backing_request_size);
        const Status     written = writer.attach_backing_pages(resource_, GuestPageList(frames, count));
        if (written != Status::ok) return written;
        uint32_t     map_info = 0u;
        const Status attached = exchange(device, request, writer.length(), map_info);
        if (attached != Status::ok) return attached;
    }
    return Status::ok;
}

Status HostVisibleBlob::map(VirtioGpuDevice &device, uint64_t offset)
{
    AV_SCOPE("HostVisibleBlob::map");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::map");
    if (resource_ == 0u || mapped_) return Status::invalid_state;
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.map_blob(resource_, offset));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, request_, writer.length(), map_info);
    if (status != Status::ok) return status;
    if (map_info != map_cache_cached && map_info != map_cache_wc) return Status::malformed;
    caching_ = map_info == map_cache_cached ? PageCaching::write_back : PageCaching::write_combined;
    offset_ = offset;
    mapped_ = true;
    return Status::ok;
}

Status HostVisibleBlob::unmap(VirtioGpuDevice &device)
{
    AV_SCOPE("HostVisibleBlob::unmap");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::unmap");
    if (resource_ == 0u || !mapped_) return Status::invalid_state;
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.unmap_blob(resource_));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, request_, writer.length(), map_info);
    if (status != Status::ok) return status;
    offset_ = 0u;
    mapped_ = false;
    return Status::ok;
}

Status HostVisibleBlob::destroy(VirtioGpuDevice &device)
{
    AV_SCOPE("HostVisibleBlob::destroy");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::destroy");
    if (resource_ == 0u) return Status::invalid_state;
    if (mapped_) {
        const Status unmapped = unmap(device);
        if (unmapped != Status::ok) return unmapped;
    }
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.unref(resource_));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, request_, writer.length(), map_info);
    if (status != Status::ok) return status;
    resource_ = 0u;
    return Status::ok;
}

Status HostVisibleBlob::exchange(VirtioGpuDevice &device, const uint8_t *request, size_t length, uint32_t &map_info)
{
    AV_SCOPE("HostVisibleBlob::exchange");
    uint32_t     written = 0u;
    const Status answered = device.ask_control(request, length, answer_, sizeof answer_, written);
    if (answered != Status::ok) return answered;
    GpuResponseReader reader;
    const Status      decoded = reader.decode(answer_, written);
    if (decoded != Status::ok) return decoded;
    if (!reader.ok()) return Status::not_supported;
    map_info = reader.map_info();
    return Status::ok;
}

void HostVisibleBlob::abandon()
{
    AV_SCOPE("HostVisibleBlob::abandon");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::abandon");
    resource_ = 0u;
    offset_ = 0u;
    mapped_ = false;
}

bool     HostVisibleBlob::created() const
{
    AV_SCOPE("HostVisibleBlob::created");
    return resource_ != 0u;
}
bool     HostVisibleBlob::mapped() const
{
    AV_SCOPE("HostVisibleBlob::mapped");
    return mapped_;
}
uint32_t HostVisibleBlob::resource() const
{
    AV_SCOPE("HostVisibleBlob::resource");
    return resource_;
}
PageCaching HostVisibleBlob::caching() const
{
    AV_SCOPE("HostVisibleBlob::caching");
    return caching_;
}

uint64_t HostVisibleBlob::offset() const
{
    AV_SCOPE("HostVisibleBlob::offset");
    return mapped_ ? offset_ : 0u;
}

bool HostVisibleBlob::invariants_hold() const
{
    AV_SCOPE("HostVisibleBlob::invariants_hold");
    return !mapped_ || resource_ != 0u;
}

}
