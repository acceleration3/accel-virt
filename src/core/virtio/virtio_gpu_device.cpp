#include "core/virtio/virtio_gpu_device.hpp"

#include "core/pci/bar_mappings.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
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
      cursor_(platform, transport_, cursor_queue, 0u, 0u, LockRank::cursor_queue), scanouts_(0u),
      running_(false) {}

/* The device must stop using the rings before the members release them. */
VirtioGpuDevice::~VirtioGpuDevice() { transport_.abandon(); }

uint16_t VirtioGpuDevice::vector_for(uint16_t source, uint32_t message_interrupts) const
{
    if (message_interrupts == 0u) return no_vector;
    return message_interrupts >= source_count ? source : 0u;
}

Status VirtioGpuDevice::start(uint32_t message_interrupts)
{
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
    if (status == Status::ok) status = transport_.start();
    if (status != Status::ok) return status;
    /* The displays the device has: the driver offers a VidPn source for each,
     * and the caller judges a count it cannot serve. */
    scanouts_ = 0u;
    static_cast<void>(transport_.read_device_u32(num_scanouts_offset, scanouts_));
    failed.dismiss();
    running_ = true;
    return Status::ok;
}

Status VirtioGpuDevice::stop()
{
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::stop");
    InvariantScope<VirtioGpuDevice> guard(platform_, *this, "stop");
    running_ = false;
    scanouts_ = 0u;
    return transport_.reset();
}

void VirtioGpuDevice::reset_immediately()
{
    platform_.require_irql_at_most(Irql::isr, "VirtioGpuDevice::reset_immediately");
    running_ = false;
    transport_.abandon();
}

uint8_t VirtioGpuDevice::read_isr()
{
    platform_.require_irql_at_most(Irql::isr, "VirtioGpuDevice::read_isr");
    return transport_.read_isr();
}

Status VirtioGpuDevice::send_control(const uint8_t *request, size_t length)
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::send_control");
    return running_ ? control_.send(request, length) : Status::invalid_state;
}

Status VirtioGpuDevice::ask_control(const uint8_t *request, size_t length, uint8_t *response, size_t capacity,
                                    uint32_t &written)
{
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::ask_control");
    if (!running_) return Status::invalid_state;
    return control_.exchange(request, length, response, capacity, written);
}

Status VirtioGpuDevice::send_cursor(const uint8_t *request, size_t length)
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::send_cursor");
    return running_ ? cursor_.send(request, length) : Status::invalid_state;
}

Status VirtioGpuDevice::harvest_control(uint32_t &completed)
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::harvest_control");
    return harvest(control_, completed, "virtio-gpu: the device refused a control request");
}

Status VirtioGpuDevice::harvest_cursor(uint32_t &completed)
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::harvest_cursor");
    return harvest(cursor_, completed, "virtio-gpu: the device refused a cursor request");
}

Status VirtioGpuDevice::harvest(GpuCommandQueue &queue, uint32_t &completed, const char *refusal)
{
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
    return running_;
}

uint32_t VirtioGpuDevice::scanouts() const
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::scanouts");
    return scanouts_;
}

uint64_t VirtioGpuDevice::features() const { return transport_.features(); }

bool VirtioGpuDevice::invariants_hold() const { return !running_ || transport_.features() != 0u; }

}  // namespace av
