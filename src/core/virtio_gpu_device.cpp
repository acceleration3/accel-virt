#include "virtio_gpu_device.hpp"

#include "bar_mappings.hpp"
#include "invariant_scope.hpp"
#include "scope_exit.hpp"
#include "virtio_pci_layout.hpp"

namespace av {

VirtioGpuDevice::VirtioGpuDevice(Platform &platform, const BarMappings &mappings, const VirtioPciLayout &layout)
    : platform_(platform),
      common_(platform, mappings.address_of(layout.common()), layout.common().length()),
      notify_(platform, mappings.address_of(layout.notify()), layout.notify().length()),
      isr_(platform, mappings.address_of(layout.isr()), layout.isr().length()),
      device_(platform, mappings.address_of(layout.device()), layout.device().length()),
      transport_(platform, common_, notify_, isr_, device_, layout.notify_multiplier()),
      control_(platform, control_queue), cursor_(platform, cursor_queue), running_(false) {}

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
        status = transport_.activate_queue(control_, vector_for(control_source, message_interrupts));
    if (status == Status::ok)
        status = transport_.activate_queue(cursor_, vector_for(cursor_source, message_interrupts));
    if (status == Status::ok) status = transport_.start();
    if (status != Status::ok) return status;
    failed.dismiss();
    running_ = true;
    return Status::ok;
}

Status VirtioGpuDevice::stop()
{
    platform_.require_irql_at_most(Irql::passive, "VirtioGpuDevice::stop");
    InvariantScope<VirtioGpuDevice> guard(platform_, *this, "stop");
    running_ = false;
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

Status VirtioGpuDevice::harvest_control(uint32_t &completed)
{
    platform_.require_irql_at_most(Irql::dispatch, "VirtioGpuDevice::harvest_control");
    InvariantScope<VirtioGpuDevice> guard(platform_, *this, "harvest_control");
    completed = 0u;
    if (!running_) return Status::invalid_state;
    for (;;) {
        uint16_t head = 0u;
        uint32_t written = 0u;
        const Status status = control_.take_used(head, written);
        if (status == Status::not_found) return Status::ok;
        if (status != Status::ok) {
            platform_.log("virtio-gpu: control queue returned a request it was never given");
            transport_.fail();
            running_ = false;
            return status;
        }
        completed++;
    }
}

uint64_t VirtioGpuDevice::features() const { return transport_.features(); }

bool VirtioGpuDevice::invariants_hold() const { return !running_ || transport_.features() != 0u; }

}  // namespace av
