#ifndef AV_SCANOUT_RESOURCE_HPP
#define AV_SCANOUT_RESOURCE_HPP

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
struct ScanoutResourceProbe;

class ScanoutResource {
public:
    static constexpr uint32_t first_resource_id = HardwareCursor::resource_id + 1u;
    static constexpr uint32_t bytes_per_pixel = 4u;

    ScanoutResource(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device, uint32_t source);
    ScanoutResource(const ScanoutResource &) = delete;
    ScanoutResource &operator=(const ScanoutResource &) = delete;

    [[nodiscard]] Status prepare(const DisplayMode &mode);
    [[nodiscard]] Status configure(const DisplayMode &mode);
    [[nodiscard]] Status disable();
    void                 release();
    void                 forget_device();
    [[nodiscard]] Status refresh(uint64_t address, const GuestPageList &pages, bool presented, uint64_t key = 0u,
                                 uint32_t frame = 0u);
    bool                 posts_frames() const;
    bool                 needs_pages(uint64_t address) const;
    bool                 scanning_out() const;
    uint32_t             width() const;
    uint32_t             height() const;
    bool                 keyed() const;
    uint32_t             page_count() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status create(VirtioGpuDevice &device);
    [[nodiscard]] Status attach(VirtioGpuDevice &device, const GuestPageList &pages);
    [[nodiscard]] Status detach(VirtioGpuDevice &device);
    [[nodiscard]] Status show(VirtioGpuDevice &device, uint32_t resource);
    [[nodiscard]] Status flush(VirtioGpuDevice &device, uint32_t frame);
    [[nodiscard]] Status unref();
    void                 report(uint64_t address, const GuestPageList &pages, Status status);

    Platform                       &platform_;
    LogBudget                      &budget_;
    ObjectStorage<VirtioGpuDevice> &device_;
    uint32_t                        source_;
    uint32_t                        width_;
    uint32_t                        height_;
    uint64_t                        attached_address_;
    uint64_t                        attached_pages_;
    uint64_t                        key_;
    uint32_t                        posted_frame_;
    uint64_t                        reported_address_;
    bool                            reported_;
    RequestRoom                     backing_request_;
    bool                            created_;
    bool                            scanning_out_;
    bool                            backed_;

    friend struct ScanoutResourceProbe;
};

}

#endif
