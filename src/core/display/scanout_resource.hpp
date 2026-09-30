/* One virtio-gpu 2D resource per display source, and the scanout it feeds: the
 * host reads the primary's pages from there, so a frame the guest composited
 * only reaches the screen once this has attached them, scanned them out and
 * asked the host to read them again. Resource ids start after the cursor's.
 * The mode is set from the VidPn commit and the address arrives at device IRQL
 * and is acted on from the DPC; DisplayScanout runs both under one lock, so
 * nothing here but prepare() may allocate. */
#ifndef AV_SCANOUT_RESOURCE_HPP
#define AV_SCANOUT_RESOURCE_HPP

#include "core/display/hardware_cursor.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/request_room.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DisplayMode;
class GuestPageList;
class LogBudget;
class VirtioGpuDevice;
struct ScanoutResourceProbe;

class ScanoutResource {
public:
    /* The cursor holds resource 1 (docs/protocol.md); source N holds this + N. */
    static constexpr uint32_t first_resource_id = HardwareCursor::resource_id + 1u;
    static constexpr uint32_t bytes_per_pixel = 4u;

    ScanoutResource(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device, uint32_t source);
    ScanoutResource(const ScanoutResource &) = delete;
    ScanoutResource &operator=(const ScanoutResource &) = delete;

    /* PASSIVE, before configure(): room for the backing request `mode` needs,
     * made where memory can be had. */
    [[nodiscard]] Status prepare(const DisplayMode &mode);
    /* The resource the committed mode needs, made once per mode. Up to
     * DISPATCH; no_memory unless prepare() made room for `mode`. */
    [[nodiscard]] Status configure(const DisplayMode &mode);
    /* The host stops showing this source; the resource is kept. Up to DISPATCH. */
    [[nodiscard]] Status disable();
    /* Tells the host to forget the resource. */
    void                 release();
    /* A new device holds none of the old one's resources. */
    void                 forget_device();
    /* The primary at `address`, backed by `pages`, and whether a frame was
     * presented since the last call: attaches and scans out an address the host
     * has not been told, and asks the host to read the pixels again when there
     * is a new frame. An address with pages the driver never recorded is not
     * scanned out: the host would read memory that is not the primary's.
     *
     * A keyed primary already shown is posted as flip `frame` whenever that is
     * a flip it has not posted, for the device's display clock to show at its
     * next vertical blank; a present into it without a flip is shown at once
     * (docs/protocol.md, "Display clock"). */
    [[nodiscard]] Status refresh(uint64_t address, const GuestPageList &pages, bool presented, uint64_t key = 0u,
                                 uint32_t frame = 0u);
    /* Whether this source's flips reach the host as numbered frames: a keyed
     * primary it is showing. Dispatch at most. */
    bool                 posts_frames() const;
    /* Whether a refresh at `address` would have to attach a backing, which is
     * the only time the primary's pages are needed. Dispatch at most: it is
     * what keeps the DPC from looking them up every idle frame. */
    bool                 needs_pages(uint64_t address) const;
    bool                 scanning_out() const;
    /* The last refresh showed a host image by key. */
    bool                 keyed() const;
    /* Pages of the frame the committed mode needs. Dispatch at most: it is
     * what the DPC asks the primary's allocation for. */
    uint32_t             page_count() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status create(VirtioGpuDevice &device);
    [[nodiscard]] Status attach(VirtioGpuDevice &device, const GuestPageList &pages);
    [[nodiscard]] Status detach(VirtioGpuDevice &device);
    [[nodiscard]] Status show(VirtioGpuDevice &device, uint32_t resource);
    /* The frame, 0 for one shown at once, is SCANOUT_IMAGE's; a primary the
     * CPU draws is transferred and has none. */
    [[nodiscard]] Status flush(VirtioGpuDevice &device, uint32_t frame);
    /* SET_SCANOUT 0 then RESOURCE_UNREF: the host holds nothing afterwards. */
    [[nodiscard]] Status unref();
    /* Where dxgkrnl says the primary is and what the driver knows about its
     * pages, logged once per address that could not be carried: a working
     * desktop says nothing, and one that never arrives says why. */
    void                 report(uint64_t address, const GuestPageList &pages, Status status);

    Platform                       &platform_;
    LogBudget                      &budget_;
    ObjectStorage<VirtioGpuDevice> &device_;
    uint32_t                        source_;
    uint32_t                        width_;
    uint32_t                        height_;
    uint64_t                        attached_address_;
    /* The fingerprint of the pages attached there: a primary made where a freed
     * one was has the same address and other pages. */
    uint64_t                        attached_pages_;
    /* The host image the primary shows, which a frame is filled from instead
     * of its pages; 0 for a primary the CPU draws. */
    uint64_t                        key_;
    /* The flip the last SCANOUT_IMAGE posted, so each is posted once. */
    uint32_t                        posted_frame_;
    /* The last address report() spoke about, so a frame that cannot be carried
     * is explained once instead of every DPC. */
    uint64_t                        reported_address_;
    bool                            reported_;
    /* A backing request covering the mode, at worst one memory entry a page;
     * grown as the mode grows, never shrunk. */
    RequestRoom                     backing_request_;
    bool                            created_;
    bool                            scanning_out_;
    /* The host's resource holds guest pages, which it must let go of before
     * it takes others. */
    bool                            backed_;

    friend struct ScanoutResourceProbe;
};

}  // namespace av

#endif  // AV_SCANOUT_RESOURCE_HPP
