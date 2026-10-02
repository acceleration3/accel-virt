/* One virtio-gpu blob per display source, and the scanout it feeds (feature
 * 058, contracts/guest_scanout_gate.md).
 *
 * The other way a source can be backed. `ScanoutResource` makes a 2D resource
 * the host holds a copy of and carries the primary's pixels into it on every
 * frame; this makes a *blob* over the primary's own pages and tells the host to
 * scan that out, so the frame is never copied: the host makes one descriptor
 * over those pages and its display samples them where they lie.
 *
 * Which of the two a source uses is decided once, from the capset bit the
 * device sets when its display can show a dma-buf, and never per frame.
 *
 * The blob can only be made once the primary's address is known, so unlike the
 * 2D resource nothing is created at the mode commit: the mode is remembered and
 * the blob is made at the first refresh that carries pages, and made again
 * whenever the address moves. */
#ifndef AV_SCANOUT_BLOB_SOURCE_HPP
#define AV_SCANOUT_BLOB_SOURCE_HPP

#include "miniport/display/hardware_cursor.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_storage.hpp"
#include "miniport/display/request_room.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DisplayMode;
class GuestPageList;
class LogBudget;
class VirtioGpuDevice;
struct ScanoutBlobSourceProbe;

class ScanoutBlobSource {
public:
    /* Above every 2D resource id, so a driver that changed its mind about a
     * source mid-run could not name one resource two ways. */
    static constexpr uint32_t first_resource_id = HardwareCursor::resource_id + 16u;
    static constexpr uint32_t bytes_per_pixel = 4u;

    ScanoutBlobSource(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device,
                      uint32_t source);
    ScanoutBlobSource(const ScanoutBlobSource &) = delete;
    ScanoutBlobSource &operator=(const ScanoutBlobSource &) = delete;

    /* PASSIVE, before configure(): room for the create-blob request `mode`
     * needs, made where memory can be had - configure() runs under the lock
     * the DPC refreshes under (DisplayScanout). */
    [[nodiscard]] Status prepare(const DisplayMode &mode);
    /* The mode this source now shows. Nothing is sent: a blob is made over the
     * primary's pages and no primary is known yet. Up to DISPATCH; no_memory
     * unless prepare() made room for `mode`. */
    [[nodiscard]] Status configure(const DisplayMode &mode);
    /* The host stops showing this source, and the blob it was shown out of is
     * released: a blob nothing scans out is one the guest may reuse the pages
     * of. Up to DISPATCH. */
    [[nodiscard]] Status disable();
    /* Tells the host to forget the blob. */
    void                 release();
    /* A new device holds none of the old one's resources. */
    void                 forget_device();
    /* The primary at `address`, backed by `pages`, and whether a frame was
     * presented since the last call: an address the host has not been told is
     * made into a blob and scanned out, and a new frame is flushed - and
     * nothing is ever transferred, which is the whole point. */
    [[nodiscard]] Status refresh(uint64_t address, const GuestPageList &pages, bool presented);
    /* Whether a refresh at `address` would have to make a blob, which is the
     * only time the primary's pages are needed. */
    bool                 needs_pages(uint64_t address) const;
    /* Pages of the frame the committed mode needs. */
    uint32_t             page_count() const;

    bool invariants_hold() const;

private:
    friend struct ScanoutBlobSourceProbe;

    [[nodiscard]] Status create(VirtioGpuDevice &device, const GuestPageList &pages);
    [[nodiscard]] Status show(VirtioGpuDevice &device, uint32_t resource);
    [[nodiscard]] Status flush(VirtioGpuDevice &device);
    /* SET_SCANOUT_BLOB 0 then RESOURCE_UNREF: the host holds nothing
     * afterwards, and the pages are the guest's again. */
    [[nodiscard]] Status unref();
    /* The one address that could not be carried, said once: a working desktop
     * says nothing and one that never arrives says why. */
    void                 report(uint64_t address, Status status);

    Platform                       &platform_;
    LogBudget                      &budget_;
    ObjectStorage<VirtioGpuDevice> &device_;
    uint32_t                        source_;
    uint32_t                        width_;
    uint32_t                        height_;
    /* The address the live blob was made over; 0 when there is none. */
    uint64_t                        blob_address_;
    /* The fingerprint of the pages the blob was made over: a primary made
     * where a freed one was has the same address and other pages. */
    uint64_t                        blob_pages_;
    uint64_t                        reported_address_;
    bool                            reported_;
    /* A create-blob request covering the mode, at worst one memory entry a
     * page; grown as the mode grows, never shrunk. */
    RequestRoom                     create_request_;
    bool                            created_;
    bool                            scanning_out_;
};

}  // namespace av

#endif  // AV_SCANOUT_BLOB_SOURCE_HPP
