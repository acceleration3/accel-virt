#include "miniport/virtio/virtio_gpu_device.hpp"

#include "miniport/pci/bar_mappings.hpp"
#include "core/platform/wait_mutex_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/virtio/virtio_pci_layout.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

namespace av {

VirtioGpuDevice::VirtioGpuDevice(Platform &platform, const BarMappings &mappings, const VirtioPciLayout &layout)
    : platform_(platform),
      common_(platform, mappings.address_of(layout.common()), layout.common().length()),
      notify_(platform, mappings.address_of(layout.notify()), layout.notify().length()),
      isr_(platform, mappings.address_of(layout.isr()), layout.isr().length()),
      device_(platform, mappings.address_of(layout.device()), layout.device().length()),
      transport_(platform, common_, notify_, isr_, device_, layout.notify_multiplier()),
      control_(platform, transport_, control_queue, GpuRequestWriter::max_response_size,
               GpuRequestWriter::max_backing_request_size, LockRank::control_queue),
      cursor_(platform, transport_, cursor_queue, 0u, 0u, LockRank::cursor_queue),
      asks_(platform, LockRank::control_exchanges), scanouts_(0u), message_interrupts_(0u), running_(false)
      {
          AV_SCOPE("VirtioGpuDevice::VirtioGpuDevice");
      }

/* The device must stop using the rings before the members release them. */
VirtioGpuDevice::~VirtioGpuDevice()
{
    AV_SCOPE("VirtioGpuDevice::~VirtioGpuDevice");
    transport_.abandon();
}

uint16_t VirtioGpuDevice::vector_for(uint16_t source, uint32_t message_interrupts) const
{
    AV_SCOPE("VirtioGpuDevice::vector_for");
    if (message_interrupts == 0u) return no_vector;
    return message_interrupts >= source_count ? source : 0u;
}

uint16_t VirtioGpuDevice::fence_vector_for(uint32_t doorbell, uint32_t message_interrupts) const
{
    AV_SCOPE("VirtioGpuDevice::fence_vector_for");
    if (message_interrupts <= source_count) return 0u;
    return static_cast<uint16_t>(source_count + doorbell % (message_interrupts - source_count));
}

void VirtioGpuDevice::assign_fence_vectors(uint32_t message_interrupts)
{
    AV_SCOPE("VirtioGpuDevice::assign_fence_vectors");
    if (message_interrupts == 0u) return;
    const uint16_t queues = transport_.queue_count();
    for (uint16_t queue = first_fence_queue; queue < queues; queue++)
        static_cast<void>(transport_.assign_queue_vector(queue, fence_vector_for(queue - first_fence_queue,
                                                                                  message_interrupts)));
}

Status VirtioGpuDevice::start(uint32_t message_interrupts)
{
    AV_SCOPE("VirtioGpuDevice::start");
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::start");
    InvariantScope<VirtioGpuDevice> guard(platform_, *this, "start");
    if (running_) return Status::invalid_state;
    Status status = transport_.negotiate(feature_edid | feature_resource_blob | feature_context_init,
                                         VirtioPciTransport::feature_version_1);
    if (status != Status::ok) return status;
    ScopeExit failed([this] { transport_.fail(); });

    if (message_interrupts != 0u) status = transport_.assign_config_vector(vector_for(config_source, message_interrupts));
    if (status == Status::ok)
        status = control_.activate(vector_for(control_source, message_interrupts));
    if (status == Status::ok) status = cursor_.activate(vector_for(cursor_source, message_interrupts));
    if (status == Status::ok) assign_fence_vectors(message_interrupts);
    if (status == Status::ok) status = transport_.start();
    if (status != Status::ok) return status;
    /* The displays the device has: the driver offers a VidPn source for each,
     * and the caller judges a count it cannot serve. */
    scanouts_ = 0u;
    static_cast<void>(transport_.read_device_u32(num_scanouts_offset, scanouts_));
    failed.dismiss();
    message_interrupts_ = message_interrupts;
    running_ = true;
    return Status::ok;
}

Status VirtioGpuDevice::stop()
{
    AV_SCOPE("VirtioGpuDevice::stop");
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::stop");
    InvariantScope<VirtioGpuDevice> guard(platform_, *this, "stop");
    running_ = false;
    scanouts_ = 0u;
    return transport_.reset();
}

void VirtioGpuDevice::reset_immediately()
{
    AV_SCOPE("VirtioGpuDevice::reset_immediately");
    platform_.require_irql_at_most(Irql::isr, "VirtioGpuDevice::reset_immediately");
    running_ = false;
    transport_.abandon();
}

uint8_t VirtioGpuDevice::read_isr()
{
    AV_SCOPE("VirtioGpuDevice::read_isr");
    platform_.require_irql_at_most(Irql::isr, "VirtioGpuDevice::read_isr");
    return transport_.read_isr();
}

Status VirtioGpuDevice::send_control(const uint8_t *request, size_t length)
{
    AV_SCOPE("VirtioGpuDevice::send_control");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::send_control");
    return running_ ? control_.send(request, length) : Status::invalid_state;
}

Status VirtioGpuDevice::send_control_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload)
{
    AV_SCOPE("VirtioGpuDevice::send_control_carrying");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::send_control_carrying");
    return running_ ? control_.send_carrying(request, length, payload) : Status::invalid_state;
}

Status VirtioGpuDevice::ask_control(const uint8_t *request, size_t length, uint8_t *response, size_t capacity,
                                    uint32_t &written)
{
    AV_SCOPE("VirtioGpuDevice::ask_control");
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::ask_control");
    if (!running_) return Status::invalid_state;
    WaitMutexGuard one(asks_);
    return control_.exchange(request, length, response, capacity, written);
}

Status VirtioGpuDevice::ask_control_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload,
                                             uint8_t *response, size_t capacity, uint32_t &written)
{
    AV_SCOPE("VirtioGpuDevice::ask_control_carrying");
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::ask_control_carrying");
    written = 0u;
    if (!running_) return Status::invalid_state;
    WaitMutexGuard one(asks_);
    return control_.exchange_carrying(request, length, payload, response, capacity, written);
}

Status VirtioGpuDevice::send_cursor(const uint8_t *request, size_t length)
{
    AV_SCOPE("VirtioGpuDevice::send_cursor");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::send_cursor");
    return running_ ? cursor_.send(request, length) : Status::invalid_state;
}

Status VirtioGpuDevice::harvest_control(uint32_t &completed)
{
    AV_SCOPE("VirtioGpuDevice::harvest_control");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::harvest_control");
    return harvest(control_, completed, "virtio-gpu: the device refused a control request");
}

Status VirtioGpuDevice::harvest_cursor(uint32_t &completed)
{
    AV_SCOPE("VirtioGpuDevice::harvest_cursor");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::harvest_cursor");
    return harvest(cursor_, completed, "virtio-gpu: the device refused a cursor request");
}

Status VirtioGpuDevice::harvest(GpuCommandQueue &queue, uint32_t &completed, const char *refusal)
{
    AV_SCOPE("VirtioGpuDevice::harvest");
    InvariantScope<VirtioGpuDevice> guard(platform_, *this, "harvest");
    completed = 0u;
    if (!running_) return Status::invalid_state;
    uint32_t refused = 0u;
    const Status status = queue.harvest(completed, refused);
    if (refused != 0u) platform_.log(refusal);
    if (status == Status::ok) return Status::ok;
    platform_.log("virtio-gpu: a queue returned a request it was never given");
    transport_.fail();
    running_ = false;
    return status;
}

bool VirtioGpuDevice::running() const
{
    AV_SCOPE("VirtioGpuDevice::running");
    return running_;
}

Status VirtioGpuDevice::acknowledge_display_event()
{
    AV_SCOPE("VirtioGpuDevice::acknowledge_display_event");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::acknowledge_display_event");
    return transport_.write_device_u32(events_clear_offset, event_display);
}

uint32_t VirtioGpuDevice::scanouts() const
{
    AV_SCOPE("VirtioGpuDevice::scanouts");
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::scanouts");
    return scanouts_;
}

uint64_t VirtioGpuDevice::features() const
{
    AV_SCOPE("VirtioGpuDevice::features");
    return transport_.features();
}

Status VirtioGpuDevice::connect_fence_queue(uint32_t doorbell, uint32_t &message)
{
    AV_SCOPE("VirtioGpuDevice::connect_fence_queue");
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::connect_fence_queue");
    message = 0u;
    if (!running_) return Status::invalid_state;
    if (doorbell >= doorbells) return Status::not_found;
    const auto queue = static_cast<uint16_t>(first_fence_queue + doorbell);
    if (message_interrupts_ == 0u) return queue < transport_.queue_count() ? Status::ok : Status::not_found;
    message = fence_vector_for(doorbell, message_interrupts_);
    /* Written again, to the value start wrote: unchanged, QEMU keeps the irqfd
     * it attached then, and a device that refused it at start refuses here. */
    return transport_.assign_queue_vector(queue, static_cast<uint16_t>(message));
}

bool VirtioGpuDevice::invariants_hold() const
{
    AV_SCOPE("VirtioGpuDevice::invariants_hold");
    return !running_ || transport_.features() != 0u;
}

}  // namespace av
