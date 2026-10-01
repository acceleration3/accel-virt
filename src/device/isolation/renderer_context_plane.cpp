#include "device/isolation/renderer_context_plane.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/renderer/blob_retired_message.hpp"
#include "proto/renderer/context_message.hpp"
#include "proto/renderer/export_image_message.hpp"
#include "proto/renderer/host_blob_message.hpp"
#include "proto/renderer/interrupt_lines_message.hpp"
#include "proto/renderer/region_message.hpp"
#include "proto/renderer/shared_released_message.hpp"
#include "proto/resources/surface_point.hpp"

namespace av {

namespace {

constexpr uint32_t protocol_version = 1u;

}  // namespace

RendererContextPlane::RendererContextPlane(Host &host, BlobTable &blobs, FenceInterrupts &interrupts)
    : host_(host), blobs_(blobs), interrupts_(interrupts), link_(host), renderer_{}, executor_{},
      executor_length_(0u), gpu_uuid_(), capset_{}, capset_length_(0u), ready_(), configured_(false), alive_(false),
      interrupts_on_(false), epoch_(0u), losses_{}, loss_count_(0u), images_{}, mirrored_{}, mirrored_count_(0u), slots_{},
      extents_{}, body_{}
{
    AV_SCOPE("RendererContextPlane::RendererContextPlane");
    for (int &image : images_) image = -1;
}

RendererContextPlane::~RendererContextPlane()
{
    AV_SCOPE("RendererContextPlane::~RendererContextPlane");
    for (ObjectStorage<RemoteContext> &slot : slots_) slot.destroy();
    drop_images();
}

Status RendererContextPlane::configure(const char *renderer, const char *executor)
{
    AV_SCOPE("RendererContextPlane::configure");
    if (renderer == nullptr || executor == nullptr) return Status::invalid_parameter;
    /* A byte past the longest path the hello carries: a path that fills it
     * was cut short. */
    TextBuffer renderer_path(renderer_, sizeof renderer_);
    TextBuffer executor_path(executor_, sizeof executor_);
    renderer_path.append(renderer);
    executor_path.append(executor);
    executor_length_ = static_cast<uint32_t>(executor_path.length());
    configured_ = renderer_path.length() != 0u && renderer_path.length() <= max_path && executor_length_ != 0u &&
                  executor_length_ <= max_path;
    return configured_ ? Status::ok : Status::invalid_parameter;
}

/* The renderer loads the library, not QEMU: a table given here is a device
 * that asked for both, which is refused. */
Status RendererContextPlane::accept(const AvExecutorOps *executor)
{
    AV_SCOPE("RendererContextPlane::accept");
    return executor == nullptr ? Status::ok : Status::invalid_parameter;
}

Status RendererContextPlane::initialize()
{
    AV_SCOPE("RendererContextPlane::initialize");
    return configured_ ? Status::ok : Status::invalid_state;
}

Status RendererContextPlane::start(const uint8_t *gpu_uuid, const CapsetInfo &capset)
{
    AV_SCOPE("RendererContextPlane::start");
    host_.require_main_loop("RendererContextPlane::start");
    if (!configured_ || gpu_uuid == nullptr) return Status::invalid_state;
    gpu_uuid_ = DeviceUuid(gpu_uuid);
    if (capset.encode(capset_, sizeof capset_) != Status::ok) return Status::invalid_parameter;
    capset_length_ = static_cast<uint32_t>(CapsetInfo::encoded_size);
    return launch();
}

/* Started, greeted, and told everything the device already holds - its
 * memory, its blobs, its interrupts - before any context reaches it. */
Status RendererContextPlane::launch()
{
    AV_SCOPE("RendererContextPlane::launch");
    Status status = host_.start_renderer(renderer_);
    if (status != Status::ok) return status;
    epoch_++;
    const HelloMessage hello(protocol_version, epoch_, gpu_uuid_, executor_, executor_length_, capset_, capset_length_);
    status = hello.encode(body_, sizeof body_);
    if (status == Status::ok) status = link_.greet(body_, static_cast<uint32_t>(hello.encoded_length()), ready_);
    if (status == Status::ok) status = send_regions();
    for (uint32_t index = 0u; index < mirrored_count_ && status == Status::ok; index++) {
        status = mirror(mirrored_[index]);
        if (status == Status::invalid_parameter || status == Status::not_supported) status = Status::ok;
    }
    if (status == Status::ok && interrupts_on_) status = send_interrupt_lines();
    if (status != Status::ok) {
        host_.kill_renderer();
        return status;
    }
    alive_ = true;
    return Status::ok;
}

Status RendererContextPlane::send_regions()
{
    AV_SCOPE("RendererContextPlane::send_regions");
    /* ROMs and device memory are regions too, and no memfd backs them: they
     * are skipped, and a guest whose RAM is one of them has no renderer. */
    const uint32_t count = host_.guest_region_count();
    uint32_t       sent = 0u;
    for (uint32_t index = 0u; index < count; index++) {
        uint32_t     region = 0u;
        int          descriptor = -1;
        uint64_t     size = 0u;
        Status       status = host_.guest_region(index, region, descriptor, size);
        if (status == Status::not_supported) continue;
        if (status != Status::ok) return status;
        sent++;
        const RegionMessage message(region, size);
        status = message.encode(body_, sizeof body_);
        if (status == Status::ok)
            status = link_.post(RendererMessageHeader::Kind::region, body_, RegionMessage::encoded_size, &descriptor, 1u);
        host_.close_descriptor(descriptor);
        if (status != Status::ok) return status;
    }
    return sent != 0u ? Status::ok : Status::not_supported;
}

uint32_t RendererContextPlane::sub_apis() const
{
    AV_SCOPE("RendererContextPlane::sub_apis");
    return ready_.sub_apis();
}

uint32_t RendererContextPlane::import_kinds(SubApi api, const uint8_t *) const
{
    AV_SCOPE("RendererContextPlane::import_kinds");
    /* A lane is a sub-API's bit number, as the Ready's sub-API mask counts
     * them. */
    for (uint32_t lane = 0u; lane < ReadyMessage::lanes; lane++)
        if (static_cast<uint32_t>(api) == 1u << lane) return ready_.import_kinds(lane);
    return 0u;
}

ObjectStorage<RemoteContext> *RendererContextPlane::slot_of(uint32_t id)
{
    AV_SCOPE("RendererContextPlane::slot_of");
    for (ObjectStorage<RemoteContext> &slot : slots_)
        if (slot.constructed() && slot.object().id() == id) return &slot;
    return nullptr;
}

const ObjectStorage<RemoteContext> *RendererContextPlane::slot_of(uint32_t id) const
{
    AV_SCOPE("RendererContextPlane::slot_of");
    for (const ObjectStorage<RemoteContext> &slot : slots_)
        if (slot.constructed() && slot.object().id() == id) return &slot;
    return nullptr;
}

/* A context made while no renderer runs has nowhere to run: the guest's
 * create fails, and its device-lost recovery tries again once one does. */
Status RendererContextPlane::create(uint32_t id)
{
    AV_SCOPE("RendererContextPlane::create");
    host_.require_main_loop("RendererContextPlane::create");
    if (id == 0u) return Status::invalid_parameter;
    if (slot_of(id) != nullptr) return Status::conflict;
    if (!alive_) return Status::invalid_state;
    for (ObjectStorage<RemoteContext> &slot : slots_) {
        if (slot.constructed()) continue;
        uint8_t       body[ContextMessage::encoded_size] = {};
        const Status  encoded = ContextMessage(id).encode(body, sizeof body);
        Status        outcome = Status::ok;
        const Status  sent = encoded == Status::ok ? link_.request(RendererMessageHeader::Kind::create_context, body,
                                                                   ContextMessage::encoded_size, nullptr, 0u, outcome)
                                                   : encoded;
        if (sent != Status::ok) {
            give_up();
            return Status::invalid_state;
        }
        if (outcome != Status::ok) return outcome;
        return slot.construct(host_, blobs_, interrupts_, id);
    }
    return Status::no_space;
}

/* A lost context is only QEMU's now: the renderer that ran it is gone. */
Status RendererContextPlane::destroy(uint32_t id)
{
    AV_SCOPE("RendererContextPlane::destroy");
    host_.require_main_loop("RendererContextPlane::destroy");
    ObjectStorage<RemoteContext> *const slot = slot_of(id);
    if (slot == nullptr) return Status::not_found;
    if (alive_ && !slot->object().lost()) {
        uint8_t body[ContextMessage::encoded_size] = {};
        Status  outcome = Status::ok;
        if (ContextMessage(id).encode(body, sizeof body) != Status::ok ||
            link_.request(RendererMessageHeader::Kind::destroy_context, body, ContextMessage::encoded_size, nullptr,
                          0u, outcome) != Status::ok)
            give_up();
    }
    slot->destroy();
    return Status::ok;
}

/* A reset takes every blob after the contexts: the renderer's copies go
 * first, so none outlives the table it copied. */
void RendererContextPlane::destroy_all()
{
    AV_SCOPE("RendererContextPlane::destroy_all");
    for (ObjectStorage<RemoteContext> &slot : slots_)
        if (slot.constructed()) static_cast<void>(destroy(slot.object().id()));
    while (mirrored_count_ != 0u) blob_released(mirrored_[mirrored_count_ - 1u]);
}

bool RendererContextPlane::holds(uint32_t id)
{
    AV_SCOPE("RendererContextPlane::holds");
    return slot_of(id) != nullptr;
}

/* The renderer's silence mid-bind is its death: the context is left unbound
 * and every other one is reported lost. A refusal - QEMU's own, of a setup
 * naming a blob that is not there or of a context already bound, or the
 * renderer's - is the guest's answer and ends nothing. */
Status RendererContextPlane::bind(uint32_t id, const ContextSetup &setup, const CapsetInfo &capset)
{
    AV_SCOPE("RendererContextPlane::bind");
    host_.require_main_loop("RendererContextPlane::bind");
    ObjectStorage<RemoteContext> *const slot = slot_of(id);
    if (slot == nullptr) return Status::invalid_parameter;
    if (!alive_) return Status::invalid_state;
    Status outcome = Status::invalid_state;
    if (slot->object().bind(setup, capset, link_, outcome) != Status::ok) {
        give_up();
        return Status::invalid_state;
    }
    return outcome;
}

bool RendererContextPlane::holds_doorbell(uint32_t index)
{
    AV_SCOPE("RendererContextPlane::holds_doorbell");
    for (const ObjectStorage<RemoteContext> &slot : slots_)
        if (slot.constructed() && slot.object().bound() && slot.object().doorbell() == index) return true;
    return false;
}

bool RendererContextPlane::can_share(uint32_t asking, uint32_t share)
{
    AV_SCOPE("RendererContextPlane::can_share");
    const ObjectStorage<RemoteContext> *const shared = asking != share ? slot_of(share) : nullptr;
    return shared != nullptr && shared->object().bound() && !shared->object().lost() &&
           shared->object().sub_api() == SubApi::opengl;
}

uint32_t RendererContextPlane::live() const
{
    AV_SCOPE("RendererContextPlane::live");
    uint32_t count = 0u;
    for (const ObjectStorage<RemoteContext> &slot : slots_)
        if (slot.constructed()) count++;
    return count;
}

void RendererContextPlane::blob_created(uint32_t id)
{
    AV_SCOPE("RendererContextPlane::blob_created");
    if (mirrored_count_ == BlobTable::max_blobs) return;
    mirrored_[mirrored_count_++] = id;
    if (!alive_) return;
    const Status status = mirror(id);
    if (status == Status::ok) return;
    /* A blob the renderer cannot be given - pages outside every region QEMU
     * sent it - is left out: every context that names it is refused, and the
     * renderer is not ended over it, which a relaunch would only repeat. */
    if (status == Status::invalid_parameter || status == Status::not_supported) {
        mirrored_count_--;
        host_.log("virtio-gpu-av: a blob outside the renderer's memory is not given to it");
        return;
    }
    give_up();
}

void RendererContextPlane::blob_released(uint32_t id)
{
    AV_SCOPE("RendererContextPlane::blob_released");
    for (uint32_t index = 0u; index < mirrored_count_; index++) {
        if (mirrored_[index] != id) continue;
        mirrored_[index] = mirrored_[--mirrored_count_];
        uint8_t body[BlobRetiredMessage::encoded_size] = {};
        if (alive_ && (BlobRetiredMessage(id).encode(body, sizeof body) != Status::ok ||
                       link_.post(RendererMessageHeader::Kind::blob_retired, body, BlobRetiredMessage::encoded_size,
                                  nullptr, 0u) != Status::ok))
            give_up();
        return;
    }
}

/* What was shared lives in the renderer's contexts, so the release goes there;
 * a renderer that is gone took everything with it. */
void RendererContextPlane::shared_released(const uint8_t *, uint64_t object_id)
{
    AV_SCOPE("RendererContextPlane::shared_released");
    if (!alive_) return;
    uint8_t body[SharedReleasedMessage::encoded_size] = {};
    static_cast<void>(SharedReleasedMessage(object_id).encode(body, sizeof body));
    if (link_.post(RendererMessageHeader::Kind::shared_released, body, SharedReleasedMessage::encoded_size, nullptr,
                   0u) != Status::ok)
        give_up();
}

Status RendererContextPlane::mirror(uint32_t id)
{
    AV_SCOPE("RendererContextPlane::mirror");
    BlobWindow pages;
    void      *host_memory = nullptr;
    Status     status = blobs_.backing(id, pages, host_memory);
    if (status != Status::ok) return Status::invalid_parameter;
    if (host_memory == nullptr) return mirror_guest(id, pages);
    int descriptor = host_.host_memory_descriptor(host_memory);
    if (descriptor < 0) return Status::not_supported;
    const HostBlobMessage message(id, pages.length());
    status = message.encode(body_, sizeof body_);
    if (status == Status::ok)
        status = link_.post(RendererMessageHeader::Kind::host_blob, body_, HostBlobMessage::encoded_size, &descriptor, 1u);
    host_.close_descriptor(descriptor);
    return status;
}

/* Each of the blob's runs is a host pointer into guest RAM; the renderer
 * finds the same pages as the region QEMU mapped them from. */
Status RendererContextPlane::mirror_guest(uint32_t id, const BlobWindow &pages)
{
    AV_SCOPE("RendererContextPlane::mirror_guest");
    const uint32_t count = pages.extents();
    if (count == 0u || count > GuestBlobMessage::max_extents) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < count; index++) {
        uint64_t          bytes = 0u;
        const void *const run = pages.extent(index, bytes);
        uint32_t          region = 0u;
        uint64_t          offset = 0u;
        const Status      located = host_.locate_guest(run, bytes, region, offset);
        if (located != Status::ok) return Status::invalid_parameter;
        extents_[index] = RendererExtent(region, offset, bytes);
    }
    const GuestBlobMessage message(id, pages.length(), extents_, count);
    const Status           status = message.encode(body_, sizeof body_);
    if (status != Status::ok) return status;
    return link_.post(RendererMessageHeader::Kind::guest_blob, body_, static_cast<uint32_t>(message.encoded_length()),
                      nullptr, 0u);
}

void RendererContextPlane::interrupts_changed(bool connected)
{
    AV_SCOPE("RendererContextPlane::interrupts_changed");
    interrupts_on_ = connected;
    if (!alive_) return;
    const Status status = connected ? send_interrupt_lines()
                                    : link_.post(RendererMessageHeader::Kind::interrupts_off, nullptr, 0u, nullptr, 0u);
    if (status != Status::ok) give_up();
}

/* One descriptor per line QEMU has a notifier for, in line order. */
Status RendererContextPlane::send_interrupt_lines()
{
    AV_SCOPE("RendererContextPlane::send_interrupt_lines");
    int      descriptors[InterruptLinesMessage::max_lines] = {};
    uint32_t count = 0u;
    uint64_t present = 0u;
    for (uint32_t line = 0u; line < InterruptLinesMessage::max_lines; line++) {
        const int descriptor = host_.fence_interrupt_descriptor(line);
        if (descriptor < 0) continue;
        descriptors[count++] = descriptor;
        present |= uint64_t{1} << line;
    }
    const InterruptLinesMessage message(InterruptLinesMessage::max_lines, present);
    Status                      status = message.encode(body_, sizeof body_);
    if (status == Status::ok)
        status = link_.post(RendererMessageHeader::Kind::interrupt_lines, body_, InterruptLinesMessage::encoded_size,
                            descriptors, count);
    for (uint32_t index = 0u; index < count; index++) host_.close_descriptor(descriptors[index]);
    return status;
}

void RendererContextPlane::give_up()
{
    AV_SCOPE("RendererContextPlane::give_up");
    if (!alive_) return;
    host_.log("virtio-gpu-av: the renderer did not answer; it is ended and its contexts lost");
    host_.kill_renderer();
    renderer_lost();
}

bool RendererContextPlane::may_restart()
{
    AV_SCOPE("RendererContextPlane::may_restart");
    const uint64_t now = host_.monotonic_nanoseconds();
    uint32_t       recent = 0u;
    for (uint32_t index = 0u; index < loss_count_; index++)
        if (now - losses_[index] < loss_window_ns) losses_[recent++] = losses_[index];
    loss_count_ = recent;
    if (loss_count_ == max_losses) return false;
    losses_[loss_count_++] = now;
    return true;
}

void RendererContextPlane::renderer_lost()
{
    AV_SCOPE("RendererContextPlane::renderer_lost");
    host_.require_main_loop("RendererContextPlane::renderer_lost");
    if (!alive_) return;
    alive_ = false;
    drop_images();
    for (ObjectStorage<RemoteContext> &slot : slots_)
        if (slot.constructed()) slot.object().lose();
    if (!may_restart()) {
        host_.log("virtio-gpu-av: the renderer keeps dying; it is not started again");
        return;
    }
    if (launch() != Status::ok) host_.log("virtio-gpu-av: the renderer could not be started again");
}

/* The display holds its own import of whatever was shown; a new renderer's
 * images are new descriptors. */
void RendererContextPlane::drop_images()
{
    AV_SCOPE("RendererContextPlane::drop_images");
    for (int &image : images_) {
        if (image >= 0) host_.close_descriptor(image);
        image = -1;
    }
}

bool RendererContextPlane::export_image(const uint8_t *, uint64_t key, AvExportedImage &image)
{
    AV_SCOPE("RendererContextPlane::export_image");
    host_.require_main_loop("RendererContextPlane::export_image");
    if (!alive_) return false;
    uint8_t body[ExportImageMessage::encoded_size] = {};
    if (ExportImageMessage(key).encode(body, sizeof body) != Status::ok) return false;
    const uint8_t *answer = nullptr;
    uint32_t       answer_length = 0u;
    int            received = -1;
    uint32_t       count = 0u;
    const Status   status = link_.exchange(RendererMessageHeader::Kind::export_image, body, sizeof body,
                                           RendererMessageHeader::Kind::exported_image, answer, answer_length,
                                           &received, 1u, count);
    if (status != Status::ok) {
        give_up();
        return false;
    }
    ExportedImageMessage exported;
    if (exported.decode(answer, answer_length, count) != Status::ok) {
        if (count != 0u) host_.close_descriptor(received);
        give_up();
        return false;
    }
    if (!exported.found()) return false;
    int &held = images_[exported.slot()];
    if (exported.fresh()) {
        if (held >= 0) host_.close_descriptor(held);
        held = received;
    }
    if (held < 0) return false;
    image.fd = held;
    image.slot = exported.slot();
    image.width = exported.width();
    image.height = exported.height();
    image.stride = exported.stride();
    image.offset = exported.offset();
    image.fourcc = exported.fourcc();
    image.reserved = 0u;
    image.modifier = exported.modifier();
    return true;
}

bool RendererContextPlane::read_image(const uint8_t *, uint64_t, void *, uint32_t, uint32_t, uint32_t)
{
    AV_SCOPE("RendererContextPlane::read_image");
    return false;
}

/* The renderer's snapshots are in its own process's memory, which the
 * engine here cannot read: presents keep their read-back. */
bool RendererContextPlane::reads_frames() const
{
    AV_SCOPE("RendererContextPlane::reads_frames");
    return false;
}

bool RendererContextPlane::map_frame(const uint8_t *, const SurfacePoint &, uint32_t, AvFrameView &)
{
    AV_SCOPE("RendererContextPlane::map_frame");
    return false;
}

void RendererContextPlane::unmap_frame(const uint8_t *, const SurfacePoint &)
{
    AV_SCOPE("RendererContextPlane::unmap_frame");
}

bool RendererContextPlane::alive() const
{
    AV_SCOPE("RendererContextPlane::alive");
    return alive_;
}

uint32_t RendererContextPlane::epoch() const
{
    AV_SCOPE("RendererContextPlane::epoch");
    return epoch_;
}

void RendererContextPlane::report(StateWriter &writer)
{
    AV_SCOPE("RendererContextPlane::report");
    writer.open("renderer", 0u);
    writer.flag("alive", alive_);
    writer.number("epoch", epoch_);
    writer.number("losses", loss_count_);
    writer.number("blobs", mirrored_count_);
    writer.number("contexts", live());
    writer.close();
}

bool RendererContextPlane::invariants_hold() const
{
    AV_SCOPE("RendererContextPlane::invariants_hold");
    if (mirrored_count_ > BlobTable::max_blobs || loss_count_ > max_losses || (alive_ && !configured_)) return false;
    for (const ObjectStorage<RemoteContext> &slot : slots_)
        if (slot.constructed() && !slot.object().invariants_hold()) return false;
    return true;
}

}  // namespace av
