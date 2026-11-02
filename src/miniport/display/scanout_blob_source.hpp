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
    static constexpr uint32_t first_resource_id = HardwareCursor::resource_id + 16u;
    static constexpr uint32_t bytes_per_pixel = 4u;

    ScanoutBlobSource(Platform &platform, LogBudget &budget, ObjectStorage<VirtioGpuDevice> &device,
                      uint32_t source);
    ScanoutBlobSource(const ScanoutBlobSource &) = delete;
    ScanoutBlobSource &operator=(const ScanoutBlobSource &) = delete;

    [[nodiscard]] Status prepare(const DisplayMode &mode);
    [[nodiscard]] Status configure(const DisplayMode &mode);
    [[nodiscard]] Status disable();
    void                 release();
    void                 forget_device();
    [[nodiscard]] Status refresh(uint64_t address, const GuestPageList &pages, bool presented);
    bool                 needs_pages(uint64_t address) const;
    uint32_t             page_count() const;

    bool invariants_hold() const;

private:
    friend struct ScanoutBlobSourceProbe;

    [[nodiscard]] Status create(VirtioGpuDevice &device, const GuestPageList &pages);
    [[nodiscard]] Status show(VirtioGpuDevice &device, uint32_t resource);
    [[nodiscard]] Status flush(VirtioGpuDevice &device);
    [[nodiscard]] Status unref();
    void                 report(uint64_t address, Status status);

    Platform                       &platform_;
    LogBudget                      &budget_;
    ObjectStorage<VirtioGpuDevice> &device_;
    uint32_t                        source_;
    uint32_t                        width_;
    uint32_t                        height_;
    uint64_t                        blob_address_;
    uint64_t                        blob_pages_;
    uint64_t                        reported_address_;
    bool                            reported_;
    RequestRoom                     create_request_;
    bool                            created_;
    bool                            scanning_out_;
};

}

#endif
