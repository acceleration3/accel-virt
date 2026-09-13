#include "core/rvk/context_blob_publisher.hpp"

#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"

namespace av {

ContextBlobPublisher::ContextBlobPublisher(Platform &platform)
    : platform_(platform), next_id_(first_resource_id), used_{}, context_ids_{}, ids_{}, counts_{},
      request_{}, answer_{} {}

Status ContextBlobPublisher::publish(VirtioGpuDevice &device, uint32_t context_id, const GuestPageList *blobs,
                                     uint32_t count, uint32_t *ids)
{
    platform_.require_irql_at_most(Irql::passive, "ContextBlobPublisher::publish");
    if (blobs == nullptr || ids == nullptr || count == 0u || count > blobs_per_context)
        return Status::invalid_parameter;
    if (slot_of(context_id) != no_slot) return Status::invalid_state;
    const uint32_t slot = free_slot();
    if (slot == no_slot) return Status::no_space;
    /* Refuse an incomplete list before creating anything the device would have
     * to be told to release. */
    for (uint32_t i = 0u; i < count; i++)
        if (!blobs[i].complete()) return Status::invalid_parameter;
    uint32_t created = 0u;
    for (uint32_t i = 0u; i < count; i++) {
        const uint32_t id = next_id_;
        const Status   status = create_blob(device, id, blobs[i]);
        if (status != Status::ok) {
            while (created != 0u) static_cast<void>(unref(device, ids[--created]));
            return status;
        }
        next_id_++;
        ids[i] = id;
        ids_[slot][i] = id;
        created++;
    }
    used_[slot] = true;
    context_ids_[slot] = context_id;
    counts_[slot] = count;
    return Status::ok;
}

Status ContextBlobPublisher::retire(VirtioGpuDevice &device, uint32_t context_id)
{
    platform_.require_irql_at_most(Irql::passive, "ContextBlobPublisher::retire");
    const uint32_t slot = slot_of(context_id);
    if (slot == no_slot) return Status::invalid_state;
    for (uint32_t i = 0u; i < counts_[slot]; i++) static_cast<void>(unref(device, ids_[slot][i]));
    used_[slot] = false;
    context_ids_[slot] = 0u;
    counts_[slot] = 0u;
    return Status::ok;
}

bool ContextBlobPublisher::holds(uint32_t context_id) const { return slot_of(context_id) != no_slot; }

uint32_t ContextBlobPublisher::slot_of(uint32_t context_id) const
{
    for (uint32_t i = 0u; i < max_contexts; i++)
        if (used_[i] && context_ids_[i] == context_id) return i;
    return no_slot;
}

uint32_t ContextBlobPublisher::free_slot() const
{
    for (uint32_t i = 0u; i < max_contexts; i++)
        if (!used_[i]) return i;
    return no_slot;
}

Status ContextBlobPublisher::create_blob(VirtioGpuDevice &device, uint32_t resource_id, const GuestPageList &pages)
{
    RvkRequestWriter writer(request_, sizeof request_);
    const Status     encoded = writer.guest_blob_pages(resource_id, pages);
    if (encoded != Status::ok) return encoded;
    return exchange(device, writer.length());
}

Status ContextBlobPublisher::unref(VirtioGpuDevice &device, uint32_t resource_id)
{
    RvkRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.unref(resource_id));
    return exchange(device, writer.length());
}

Status ContextBlobPublisher::exchange(VirtioGpuDevice &device, size_t length)
{
    uint32_t     written = 0u;
    const Status answered = device.ask_control(request_, length, answer_, sizeof answer_, written);
    if (answered != Status::ok) return answered;
    GpuResponseReader reader;
    const Status      decoded = reader.decode(answer_, written);
    if (decoded != Status::ok) return decoded;
    return reader.ok() ? Status::ok : Status::not_supported;
}

bool ContextBlobPublisher::invariants_hold() const
{
    for (uint32_t i = 0u; i < max_contexts; i++)
        if (used_[i] && (counts_[i] == 0u || counts_[i] > blobs_per_context)) return false;
    return next_id_ >= first_resource_id;
}

}  // namespace av
