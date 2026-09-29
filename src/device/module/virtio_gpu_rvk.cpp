#include "device/module/virtio_gpu_rvk.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/control/display_mode_commands.hpp"
#include "device/control/blob_commands.hpp"
#include "device/control/context_commands.hpp"
#include "device/control/scanout_commands.hpp"
#include "device/control/present_commands.hpp"
#include "device/control/shared_commands.hpp"
#include "device/control/control_request.hpp"
#include "device/execution/executor_library.hpp"
#include "device/host/host.hpp"
#include "core/support/text_buffer.hpp"

namespace av {

/* Conventional low RAM: present on every machine before the guest runs, and the
 * import probe puts back what it found there. */
constexpr uint64_t guest_probe_address = 0x100000u;

VirtioGpuRvk::VirtioGpuRvk(Host &host)
    : host_(host), binding_(host), pci_(host), feasibility_(host), capset_(host), recorder_(host), blobs_(host),
      heads_(host, blobs_), display_modes_(host), interrupts_(host), status_(host, interrupts_), local_(host, blobs_, interrupts_, recorder_),
      remote_(host, blobs_, interrupts_), contexts_(&local_)
      {
          AV_SCOPE("VirtioGpuRvk::VirtioGpuRvk");
      }

Status VirtioGpuRvk::realize(const char *host_gpu, bool base_blobs, const RvkExecutorOps *executor,
                             const RvkRecordingOptions *recording, const char *renderer, const char *executor_path,
                             TextBuffer &message)
{
    AV_SCOPE("VirtioGpuRvk::realize");
    host_.require_main_loop("VirtioGpuRvk::realize");
    if (binding_.bound()) return Status::invalid_state;
    if (base_blobs) {
        message.append("virtio-gpu-rvk: blob=on is not supported; the device answers blob requests itself");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    const bool isolated = renderer != nullptr && renderer[0] != '\0';
    contexts_ = isolated ? static_cast<ContextPlane *>(&remote_) : &local_;
    if (isolated && remote_.configure(renderer, executor_path) != Status::ok) {
        message.append("virtio-gpu-rvk: renderer= needs executor= naming the library it loads, each a path it can carry");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    if (contexts_->accept(executor) == Status::invalid_parameter) {
        message.append("virtio-gpu-rvk: the executor library's table is not ABI version 1 to 4, or lacks an operation");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    /* A realize that failed before may already have the lock. */
    if (interrupts_.initialize() == Status::no_memory) {
        message.append("virtio-gpu-rvk: the host has no mutex for fence interrupts");
        host_.log(message.text());
        return Status::no_memory;
    }
    if (blobs_.initialize() == Status::no_memory) {
        message.append("virtio-gpu-rvk: the host has no mutex for the blob table");
        host_.log(message.text());
        return Status::no_memory;
    }
    if (contexts_->initialize() == Status::no_memory) {
        message.append("virtio-gpu-rvk: the host has no mutex for the contexts' shared objects and executors");
        host_.log(message.text());
        return Status::no_memory;
    }
    const Status status = binding_.bind(host_gpu, message);
    if (status != Status::ok) return status;
    pci_.read(binding_.index());
    /* What this host can actually do, asked once, on the GPU just bound: a
     * probe that fails leaves its bit clear and the device starts anyway. */
    feasibility_.decide(guest_probe_address);
    if (feasibility_.interrupts().initialize() == Status::ok) interrupts_.report_to(feasibility_.interrupts());
    /* A renderer answers what its library offers only once started, and it is
     * started with a capset: one offering every sub-API, which QEMU narrows
     * to the answer before any guest reads it. QEMU checks each context's
     * setup against the narrowed one before the renderer sees it. */
    if (isolated) {
        capset_.describe(binding_.index(), ExecutorLibrary::known_sub_apis, feasibility_.features());
        const Status started = contexts_->start(capset_.info().gpu().uuid(), capset_.info());
        if (started != Status::ok) {
            message.append("virtio-gpu-rvk: the renderer did not start, or did not answer: status ");
            message.append_decimal(static_cast<uint32_t>(started));
            host_.log(message.text());
            return started;
        }
    }
    capset_.describe(binding_.index(), contexts_->sub_apis(), feasibility_.features());
    /* The probe proved this device can hand a guest blob to a host API; only
     * the library knows whether the API will take one. Both have to be true
     * before the guest is told its memory arrives without a copy. */
    if (capset_.described() &&
        contexts_->import_kinds(SubApi::vulkan, capset_.info().gpu().uuid()) == 0u)
        capset_.withdraw_guest_import();
    return start_recording(recording, message);
}

/* After the GPU is bound, so the recording's header names the GPU it recorded
 * and the version its contexts will speak. */
Status VirtioGpuRvk::start_recording(const RvkRecordingOptions *recording, TextBuffer &message)
{
    AV_SCOPE("VirtioGpuRvk::start_recording");
    if (recording == nullptr) return Status::ok;
    const AtCeiling at_ceiling = recording->wrap != 0 ? AtCeiling::wrap : AtCeiling::stop;
    const Status    opened = recorder_.open(recording->path, recording->limit_bytes, recording->kinds,
                                            at_ceiling, capset_.info().gpu(), capset_.info().version());
    if (opened == Status::ok) return Status::ok;
    message.append("virtio-gpu-rvk: record-kinds names no record kind, or record-limit is below 64 bytes");
    host_.log(message.text());
    return opened;
}

void VirtioGpuRvk::reset()
{
    AV_SCOPE("VirtioGpuRvk::reset");
    host_.require_main_loop("VirtioGpuRvk::reset");
    contexts_->destroy_all();
    /* The guest's shared-object ids start again, so what the executor still
     * shares under the old ones goes - on this device's GPU, which the device
     * knows and a plane running in QEMU was never told. */
    contexts_->shared_released(capset_.info().gpu().uuid(), 0u);
    const bool connected = interrupts_.connected();
    status_.reset();
    if (connected && !interrupts_.connected()) contexts_->interrupts_changed(false);
    /* Before the blobs: a head holds a pin on the blob it shows, so every head
     * lets go before the table it pinned from does. */
    heads_.release_all();
    blobs_.release_all();
}

/* An unrealized device has no lock to hold its interrupts apart, so a status
 * written before realize would only log a refusal it can do nothing about. */
void VirtioGpuRvk::status(uint8_t status)
{
    AV_SCOPE("VirtioGpuRvk::status");
    host_.require_main_loop("VirtioGpuRvk::status");
    if (!binding_.bound()) return;
    const bool connected = interrupts_.connected();
    status_.set(status);
    if (interrupts_.connected() != connected) contexts_->interrupts_changed(interrupts_.connected());
}

/* The glue watches the renderer's process and socket; either ending lands
 * here once. */
void VirtioGpuRvk::renderer_lost()
{
    AV_SCOPE("VirtioGpuRvk::renderer_lost");
    host_.require_main_loop("VirtioGpuRvk::renderer_lost");
    contexts_->renderer_lost();
}

bool VirtioGpuRvk::control(void *request, uint32_t type)
{
    AV_SCOPE("VirtioGpuRvk::control");
    host_.require_main_loop("VirtioGpuRvk::control");
    if (!binding_.bound()) return false;
    /* The first request the guest sends arrives long after QEMU has started
     * its display, which is the earliest the display's answer is worth
     * anything (DeviceCapset::resolve_display). */
    /* The scanout shows host images by key (SCANOUT_IMAGE), which a blob's
     * dma-buf cannot carry, so the guest keeps the 2D path even where the
     * display could take a dma-buf - a GL window, which then only draws. */
    if (!capset_.display_resolved()) capset_.resolve_display(false);
    ControlRequest control(host_, request);
    return ContextCommands(host_, *contexts_, capset_).answer(control, type) ||
           BlobCommands(host_, blobs_, recorder_, *contexts_).answer(control, type) ||
           ScanoutCommands(host_, blobs_, heads_).answer(control, type) ||
           SharedCommands(host_, *contexts_, capset_.info().gpu().uuid()).answer(control, type) ||
           PresentCommands(host_).answer(control, type) ||
           DisplayModeCommands(host_, display_modes_).answer(control, type);
}

Status VirtioGpuRvk::set_modes(const char *text, uint32_t &width, uint32_t &height)
{
    AV_SCOPE("VirtioGpuRvk::set_modes");
    host_.require_main_loop("VirtioGpuRvk::set_modes");
    width = 0u;
    height = 0u;
    const Status set = display_modes_.set(text);
    if (set != Status::ok) return set;
    const DisplayModeSet &modes = display_modes_.current();
    width = modes.width(modes.preferred());
    height = modes.height(modes.preferred());
    return Status::ok;
}

int VirtioGpuRvk::offer_modes(const char *text, uint32_t *width, uint32_t *height)
{
    AV_SCOPE("VirtioGpuRvk::offer_modes");
    uint32_t     preferred_width = 0u;
    uint32_t     preferred_height = 0u;
    const Status set = set_modes(text, preferred_width, preferred_height);
    if (width != nullptr) *width = preferred_width;
    if (height != nullptr) *height = preferred_height;
    if (set == Status::invalid_parameter) return -1;
    return set == Status::ok ? 1 : 0;
}

uint32_t VirtioGpuRvk::capsets() const
{
    AV_SCOPE("VirtioGpuRvk::capsets");
    return capset_.described() ? 1u : 0u;
}

bool VirtioGpuRvk::pci_identity(RvkPciIdentity &identity) const
{
    AV_SCOPE("VirtioGpuRvk::pci_identity");
    host_.require_main_loop("VirtioGpuRvk::pci_identity");
    return pci_.presented(identity);
}

uint32_t VirtioGpuRvk::blobs() const
{
    AV_SCOPE("VirtioGpuRvk::blobs");
    return blobs_.live();
}

uint32_t VirtioGpuRvk::scanouts() const
{
    AV_SCOPE("VirtioGpuRvk::scanouts");
    return heads_.showing();
}

/* A context reports its fences by interrupt, so one created while nothing
 * carries them would run with no one to tell. */
Status VirtioGpuRvk::create_context(uint32_t id)
{
    AV_SCOPE("VirtioGpuRvk::create_context");
    host_.require_main_loop("VirtioGpuRvk::create_context");
    if (!binding_.bound() || !interrupts_.connected()) return Status::invalid_state;
    return contexts_->create(id);
}

Status VirtioGpuRvk::destroy_context(uint32_t id)
{
    AV_SCOPE("VirtioGpuRvk::destroy_context");
    host_.require_main_loop("VirtioGpuRvk::destroy_context");
    return contexts_->destroy(id);
}

uint32_t VirtioGpuRvk::contexts() const
{
    AV_SCOPE("VirtioGpuRvk::contexts");
    return contexts_->live();
}

bool VirtioGpuRvk::realized() const
{
    AV_SCOPE("VirtioGpuRvk::realized");
    return binding_.bound();
}

bool VirtioGpuRvk::export_image(uint64_t key, RvkExportedImage &image)
{
    AV_SCOPE("VirtioGpuRvk::export_image");
    if (!realized()) return false;
    return contexts_->export_image(capset_.info().gpu().uuid(), key, image);
}

bool VirtioGpuRvk::read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height)
{
    AV_SCOPE("VirtioGpuRvk::read_image");
    if (!realized()) return false;
    return contexts_->read_image(capset_.info().gpu().uuid(), key, pixels, stride, width, height);
}

uint32_t VirtioGpuRvk::gpu_index() const
{
    AV_SCOPE("VirtioGpuRvk::gpu_index");
    return binding_.index();
}

void VirtioGpuRvk::report(StateWriter &writer)
{
    AV_SCOPE("VirtioGpuRvk::report");
    writer.flag("realized", realized());
    if (realized()) {
        writer.number("gpu", gpu_index());
        writer.number("capsets", capsets());
        writer.number("blobs", blobs());
        writer.number("scanouts", scanouts());
        writer.number("contexts", contexts());
        writer.flag("driver_ok", (status_.value() & DriverStatus::driver_ok) != 0u);
    }
    heads_.report(writer);
    contexts_->report(writer);
}

/* Contexts exist only on a realized device. */
bool VirtioGpuRvk::invariants_hold() const
{
    AV_SCOPE("VirtioGpuRvk::invariants_hold");
    return contexts_->invariants_hold() && blobs_.invariants_hold() &&
           heads_.invariants_hold() && interrupts_.invariants_hold() &&
           status_.invariants_hold() && (binding_.bound() || contexts_->live() == 0u);
}

}  // namespace av
