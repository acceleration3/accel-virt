#include "device/module/virtio_gpu_av.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/control/display_clock_commands.hpp"
#include "device/control/scanout_plane_commands.hpp"
#include "device/control/display_mode_commands.hpp"
#include "device/control/blob_commands.hpp"
#include "device/control/context_commands.hpp"
#include "device/control/scanout_commands.hpp"
#include "device/control/present_commands.hpp"
#include "device/control/present_engine_commands.hpp"
#include "device/present/present_frames.hpp"
#include "device/control/shared_commands.hpp"
#include "device/control/control_request.hpp"
#include "device/abi/entry_points.hpp"
#include "device/execution/executor_library.hpp"
#include "device/host/host.hpp"
#include "core/support/text_buffer.hpp"
#include "device/gpu/pci_address.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/export/export_greeting.hpp"

namespace av {

VirtioGpuAv::VirtioGpuAv(Host &host)
    : host_(host), binding_(host), pci_(host), feasibility_(host), capset_(host), recorder_(host), blobs_(host),
      host_blobs_(host), heads_(host, blobs_), display_modes_(host), interrupts_(host),
      clock_(host, interrupts_, &AvDisplayFramesShow, this), planes_(host), presents_(host, interrupts_), status_(host, interrupts_), local_(host, blobs_, host_blobs_, interrupts_, recorder_),
      remote_(host, blobs_, interrupts_), contexts_(&local_), exports_(host, clock_)
      {
          AV_SCOPE("VirtioGpuAv::VirtioGpuAv");
      }

void VirtioGpuAv::share_lendings(GuestRamLendings &lendings)
{
    AV_SCOPE("VirtioGpuAv::share_lendings");
    host_blobs_.share_lendings(lendings);
}

Status VirtioGpuAv::realize(const char *host_gpu, bool base_blobs, const AvExecutorOps *executor,
                             const AvRecordingOptions *recording, const char *renderer, const char *executor_path,
                             TextBuffer &message)
{
    AV_SCOPE("VirtioGpuAv::realize");
    host_.require_main_loop("VirtioGpuAv::realize");
    if (binding_.bound()) return Status::invalid_state;
    if (base_blobs) {
        message.append("virtio-gpu-av: blob=on is not supported; the device answers blob requests itself");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    const bool isolated = renderer != nullptr && renderer[0] != '\0';
    contexts_ = isolated ? static_cast<ContextPlane *>(&remote_) : &local_;
    if (isolated && remote_.configure(renderer, executor_path) != Status::ok) {
        message.append("virtio-gpu-av: renderer= needs executor= naming the library it loads, each a path it can carry");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    if (contexts_->accept(executor) == Status::invalid_parameter) {
        message.append("virtio-gpu-av: the executor library's table is not this ABI version, or lacks an operation");
        host_.log(message.text());
        return Status::invalid_parameter;
    }
    if (interrupts_.initialize() == Status::no_memory) {
        message.append("virtio-gpu-av: the host has no mutex for fence interrupts");
        host_.log(message.text());
        return Status::no_memory;
    }
    if (blobs_.initialize() == Status::no_memory) {
        message.append("virtio-gpu-av: the host has no mutex for the blob table");
        host_.log(message.text());
        return Status::no_memory;
    }
    if (host_blobs_.initialize(*contexts_) == Status::no_memory) {
        message.append("virtio-gpu-av: the host has no mutex for the host blob table");
        host_.log(message.text());
        return Status::no_memory;
    }
    if (contexts_->initialize() == Status::no_memory) {
        message.append("virtio-gpu-av: the host has no mutex for the contexts' ordering, pulse and executors");
        host_.log(message.text());
        return Status::no_memory;
    }
    const Status status = binding_.bind(host_gpu, message);
    if (status != Status::ok) return status;
    pci_.read(binding_.index());
    if (feasibility_.interrupts().initialize() == Status::ok) interrupts_.report_to(feasibility_.interrupts());
    const uint64_t features = feasibility_.features() | CapsetInfo::feature_present_engine | CapsetInfo::feature_raises |
                              (contexts_->reads_frames() ? CapsetInfo::feature_present_from_image : 0u) |
                              (contexts_->composes() ? CapsetInfo::feature_scanout_planes : 0u);
    if (isolated) {
        capset_.describe(binding_.index(), ExecutorLibrary::known_sub_apis, features);
        const Status started = contexts_->start(capset_.info().gpu().uuid(), capset_.info());
        if (started != Status::ok) {
            message.append("virtio-gpu-av: the renderer did not start, or did not answer: status ");
            message.append_decimal(static_cast<uint32_t>(started));
            host_.log(message.text());
            return started;
        }
    }
    capset_.describe(binding_.index(), contexts_->sub_apis(), features);
    if (!isolated) static_cast<void>(contexts_->start(capset_.info().gpu().uuid(), capset_.info()));
    attach_exports();
    if (clock_.initialize() == Status::no_memory || presents_.initialize() == Status::no_memory ||
        planes_.initialize() == Status::no_memory) {
        message.append("virtio-gpu-av: the host has no mutex or event for the display clock, the present engine or"
                       " the scanout planes");
        host_.log(message.text());
        return Status::no_memory;
    }
    return start_recording(recording, message);
}

void VirtioGpuAv::attach_exports()
{
    AV_SCOPE("VirtioGpuAv::attach_exports");
    const uint8_t *const uuid = capset_.info().gpu().uuid();
    PciAddress           address;
    const bool           on_pci = host_.gpu_pci_address(binding_.index(), address) == Status::ok;
    exports_.attach(*contexts_, uuid, on_pci ? address.greeting(uuid) : ExportGreeting(uuid), planes_);
}

Status VirtioGpuAv::start_recording(const AvRecordingOptions *recording, TextBuffer &message)
{
    AV_SCOPE("VirtioGpuAv::start_recording");
    if (recording == nullptr) return Status::ok;
    const AtCeiling at_ceiling = recording->wrap != 0 ? AtCeiling::wrap : AtCeiling::stop;
    const Status    opened = recorder_.open(recording->path, recording->limit_bytes, recording->kinds,
                                            at_ceiling, capset_.info().gpu(), capset_.info().version());
    if (opened == Status::ok) return Status::ok;
    message.append("virtio-gpu-av: record-kinds names no record kind, or record-limit is below 64 bytes");
    host_.log(message.text());
    return opened;
}

void VirtioGpuAv::reset()
{
    AV_SCOPE("VirtioGpuAv::reset");
    host_.require_main_loop("VirtioGpuAv::reset");
    clock_.detach();
    planes_.clear();
    presents_.detach();
    contexts_->destroy_all();
    contexts_->shared_released(capset_.info().gpu().uuid(), 0u);
    exports_.revoke(0u, ExportRevocation::reset);
    const bool connected = interrupts_.connected();
    status_.reset();
    if (connected && !interrupts_.connected()) contexts_->interrupts_changed(false);
    heads_.release_all();
    host_blobs_.release_all();
    blobs_.release_all();
}

void VirtioGpuAv::status(uint8_t status)
{
    AV_SCOPE("VirtioGpuAv::status");
    host_.require_main_loop("VirtioGpuAv::status");
    if (!binding_.bound()) return;
    const bool connected = interrupts_.connected();
    status_.set(status);
    if (interrupts_.connected() != connected) contexts_->interrupts_changed(interrupts_.connected());
}

void VirtioGpuAv::renderer_lost()
{
    AV_SCOPE("VirtioGpuAv::renderer_lost");
    host_.require_main_loop("VirtioGpuAv::renderer_lost");
    contexts_->renderer_lost();
}

bool VirtioGpuAv::control(void *request, uint32_t type)
{
    AV_SCOPE("VirtioGpuAv::control");
    host_.require_main_loop("VirtioGpuAv::control");
    if (!binding_.bound()) return false;
    if (!capset_.display_resolved()) capset_.resolve_display(false);
    ControlRequest         control(host_, request);
    const PresentFrames snapshots(*contexts_, capset_.info().gpu().uuid());
    return ContextCommands(host_, *contexts_, capset_).answer(control, type) ||
           BlobCommands(host_, blobs_, host_blobs_, recorder_, *contexts_).answer(control, type) ||
           ScanoutCommands(host_, blobs_, heads_).answer(control, type) ||
           SharedCommands(host_, *contexts_, exports_, capset_.info().gpu().uuid()).answer(control, type) ||
           PresentCommands(host_).answer(control, type) ||
           DisplayModeCommands(host_, display_modes_).answer(control, type) ||
           DisplayClockCommands(host_, clock_).answer(control, type) ||
           ScanoutPlaneCommands(host_, planes_).answer(control, type) ||
           PresentEngineCommands(host_, presents_, snapshots).answer(control, type);
}

bool VirtioGpuAv::export_accepted(int connection, bool same_user)
{
    AV_SCOPE("VirtioGpuAv::export_accepted");
    host_.require_main_loop("VirtioGpuAv::export_accepted");
    return realized() && exports_.accept(connection, same_user) == Status::ok;
}

Status VirtioGpuAv::set_modes(const char *text, uint32_t &width, uint32_t &height)
{
    AV_SCOPE("VirtioGpuAv::set_modes");
    host_.require_main_loop("VirtioGpuAv::set_modes");
    width = 0u;
    height = 0u;
    const Status set = display_modes_.set(text);
    if (set != Status::ok) return set;
    const DisplayModeSet &modes = display_modes_.current();
    width = modes.width(modes.preferred());
    height = modes.height(modes.preferred());
    return Status::ok;
}

int VirtioGpuAv::offer_modes(const char *text, uint32_t *width, uint32_t *height)
{
    AV_SCOPE("VirtioGpuAv::offer_modes");
    uint32_t     preferred_width = 0u;
    uint32_t     preferred_height = 0u;
    const Status set = set_modes(text, preferred_width, preferred_height);
    if (width != nullptr) *width = preferred_width;
    if (height != nullptr) *height = preferred_height;
    if (set == Status::invalid_parameter) return -1;
    return set == Status::ok ? 1 : 0;
}

Status VirtioGpuAv::set_refresh(uint32_t millihz)
{
    AV_SCOPE("VirtioGpuAv::set_refresh");
    host_.require_main_loop("VirtioGpuAv::set_refresh");
    const Status set = display_modes_.set_refresh(millihz);
    if (set != Status::ok) return set;
    return clock_.retime_all(millihz);
}

bool VirtioGpuAv::offer_refresh(uint32_t millihz)
{
    AV_SCOPE("VirtioGpuAv::offer_refresh");
    return set_refresh(millihz) == Status::ok;
}

uint32_t VirtioGpuAv::capsets() const
{
    AV_SCOPE("VirtioGpuAv::capsets");
    return capset_.described() ? 1u : 0u;
}

bool VirtioGpuAv::pci_identity(AvPciIdentity &identity) const
{
    AV_SCOPE("VirtioGpuAv::pci_identity");
    host_.require_main_loop("VirtioGpuAv::pci_identity");
    return pci_.presented(identity);
}

uint32_t VirtioGpuAv::blobs() const
{
    AV_SCOPE("VirtioGpuAv::blobs");
    return blobs_.live();
}

uint32_t VirtioGpuAv::scanouts() const
{
    AV_SCOPE("VirtioGpuAv::scanouts");
    return heads_.showing();
}

Status VirtioGpuAv::create_context(uint32_t id, const char *name)
{
    AV_SCOPE("VirtioGpuAv::create_context");
    host_.require_main_loop("VirtioGpuAv::create_context");
    if (!binding_.bound() || !interrupts_.connected()) return Status::invalid_state;
    return contexts_->create(id, name);
}

Status VirtioGpuAv::destroy_context(uint32_t id)
{
    AV_SCOPE("VirtioGpuAv::destroy_context");
    host_.require_main_loop("VirtioGpuAv::destroy_context");
    return contexts_->destroy(id);
}

uint32_t VirtioGpuAv::contexts() const
{
    AV_SCOPE("VirtioGpuAv::contexts");
    return contexts_->live();
}

bool VirtioGpuAv::realized() const
{
    AV_SCOPE("VirtioGpuAv::realized");
    return binding_.bound();
}

bool VirtioGpuAv::export_image(uint64_t key, AvExportedImage &image)
{
    AV_SCOPE("VirtioGpuAv::export_image");
    if (!realized()) return false;
    return contexts_->export_image(capset_.info().gpu().uuid(), key, image);
}

bool VirtioGpuAv::export_head(uint32_t head, uint64_t key, AvExportedImage &image)
{
    AV_SCOPE("VirtioGpuAv::export_head");
    if (!realized()) return false;
    if (!planes_.composed(head)) return key != 0u && export_image(key, image);
    AvScanoutPlane planes[HeadPlanes::layers] = {};
    uint32_t       width = 0u;
    uint32_t       height = 0u;
    const uint32_t count = planes_.planes(head, planes, width, height);
    return contexts_->compose_planes(capset_.info().gpu().uuid(), planes, count, width, height, image);
}

bool VirtioGpuAv::post_frame(uint32_t head, uint32_t frame, uint64_t key)
{
    AV_SCOPE("VirtioGpuAv::post_frame");
    host_.require_main_loop("VirtioGpuAv::post_frame");
    if (frame == 0u || !clock_.latches(head) || !host_.display_takes_dmabuf(head)) return false;
    clock_.post(head, frame, key);
    return true;
}

void VirtioGpuAv::show_latched_frames()
{
    AV_SCOPE("VirtioGpuAv::show_latched_frames");
    host_.require_main_loop("VirtioGpuAv::show_latched_frames");
    clock_.showing();
    for (uint32_t head = 0u; head < DisplayClock::heads; head++) {
        uint64_t         key = 0u;
        AvExportedImage image = {};
        if (!clock_.take_unshown(head, key) || !export_head(head, key, image)) continue;
        static_cast<void>(host_.show_image(head, image));
    }
}

bool VirtioGpuAv::read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height)
{
    AV_SCOPE("VirtioGpuAv::read_image");
    if (!realized()) return false;
    return contexts_->read_image(capset_.info().gpu().uuid(), key, pixels, stride, width, height);
}

uint32_t VirtioGpuAv::gpu_index() const
{
    AV_SCOPE("VirtioGpuAv::gpu_index");
    return binding_.index();
}

void VirtioGpuAv::report(StateWriter &writer)
{
    AV_SCOPE("VirtioGpuAv::report");
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
    host_blobs_.report(writer);
    contexts_->report(writer);
}

bool VirtioGpuAv::invariants_hold() const
{
    AV_SCOPE("VirtioGpuAv::invariants_hold");
    return contexts_->invariants_hold() && blobs_.invariants_hold() && host_blobs_.invariants_hold() &&
           heads_.invariants_hold() && interrupts_.invariants_hold() && presents_.invariants_hold() &&
           status_.invariants_hold() && exports_.invariants_hold() &&
           (binding_.bound() || (contexts_->live() == 0u && exports_.live() == 0u));
}

}
