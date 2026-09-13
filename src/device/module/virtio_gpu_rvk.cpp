#include "device/module/virtio_gpu_rvk.hpp"

#include "device/control/blob_commands.hpp"
#include "device/control/context_commands.hpp"
#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "core/support/text_buffer.hpp"

namespace av {

/* Conventional low RAM: present on every machine before the guest runs, and the
 * import probe puts back what it found there. */
constexpr uint64_t guest_probe_address = 0x100000u;

VirtioGpuRvk::VirtioGpuRvk(Host &host)
    : host_(host), binding_(host), library_(), feasibility_(host), capset_(host), blobs_(host), interrupts_(host),
      status_(host, interrupts_), contexts_(host, blobs_, interrupts_, library_) {}

Status VirtioGpuRvk::realize(const char *host_gpu, bool base_blobs, const RvkExecutorOps *executor,
                             TextBuffer &message)
{
    host_.require_main_loop("VirtioGpuRvk::realize");
    if (binding_.bound()) return Status::invalid_state;
    if (base_blobs) {
        message.append("virtio-gpu-rvk: blob=on is not supported; the device answers blob requests itself");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    if (library_.accept(executor) == Status::invalid_parameter) {
        message.append("virtio-gpu-rvk: the executor library's table is not ABI version 1 or lacks an operation");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    /* A realize that failed before may already have the lock. */
    if (interrupts_.initialize() == Status::no_memory) {
        message.append("virtio-gpu-rvk: the host has no mutex for fence interrupts");
        host_.log(message.text());
        return Status::no_memory;
    }
    const Status status = binding_.bind(host_gpu, message);
    if (status != Status::ok) return status;
    /* What this host can actually do, asked once, on the GPU just bound: a
     * probe that fails leaves its bit clear and the device starts anyway. */
    feasibility_.decide(guest_probe_address);
    capset_.describe(binding_.index(), library_.sub_apis(), feasibility_.features());
    return status;
}

void VirtioGpuRvk::reset()
{
    host_.require_main_loop("VirtioGpuRvk::reset");
    contexts_.destroy_all();
    status_.reset();
    blobs_.release_all();
}

/* An unrealized device has no lock to hold its interrupts apart, so a status
 * written before realize would only log a refusal it can do nothing about. */
void VirtioGpuRvk::status(uint8_t status)
{
    host_.require_main_loop("VirtioGpuRvk::status");
    if (!binding_.bound()) return;
    status_.set(status);
}

bool VirtioGpuRvk::control(void *request, uint32_t type)
{
    host_.require_main_loop("VirtioGpuRvk::control");
    if (!binding_.bound()) return false;
    ControlRequest control(host_, request);
    return ContextCommands(host_, contexts_, capset_).answer(control, type) ||
           BlobCommands(host_, blobs_).answer(control, type);
}

uint32_t VirtioGpuRvk::capsets() const
{
    return capset_.described() ? 1u : 0u;
}

uint32_t VirtioGpuRvk::blobs() const
{
    return blobs_.live();
}

/* A context reports its fences by interrupt, so one created while nothing
 * carries them would run with no one to tell. */
Status VirtioGpuRvk::create_context(uint32_t id)
{
    host_.require_main_loop("VirtioGpuRvk::create_context");
    if (!binding_.bound() || !interrupts_.connected()) return Status::invalid_state;
    return contexts_.create(id);
}

Status VirtioGpuRvk::destroy_context(uint32_t id)
{
    host_.require_main_loop("VirtioGpuRvk::destroy_context");
    return contexts_.destroy(id);
}

uint32_t VirtioGpuRvk::contexts() const
{
    return contexts_.live();
}

bool VirtioGpuRvk::realized() const
{
    return binding_.bound();
}

uint32_t VirtioGpuRvk::gpu_index() const
{
    return binding_.index();
}

/* Contexts exist only on a realized device. */
bool VirtioGpuRvk::invariants_hold() const
{
    return contexts_.invariants_hold() && blobs_.invariants_hold() && interrupts_.invariants_hold() &&
           status_.invariants_hold() && (binding_.bound() || contexts_.live() == 0u);
}

}  // namespace av
