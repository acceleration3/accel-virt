/* One virtio-gpu 2D resource per display source, and the scanout it feeds: the
 * host reads the primary's pages from there, so a frame the guest composited
 * only reaches the screen once this has attached them, scanned them out and
 * asked the host to read them again. Resource ids start after the cursor's.
 * The mode is set at PASSIVE from the VidPn commit; the address arrives at
 * device IRQL and is acted on from the DPC, which is why refresh() is the only
 * method reachable above PASSIVE. */
#ifndef AV_SCANOUT_RESOURCE_HPP
#define AV_SCANOUT_RESOURCE_HPP

#include "core/display/hardware_cursor.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DisplayMode;
class VirtioGpuDevice;
struct ScanoutResourceProbe;

class ScanoutResource {
public:
    /* The cursor holds resource 1 (docs/protocol.md); source N holds this + N. */
    static constexpr uint32_t first_resource_id = HardwareCursor::resource_id + 1u;
    static constexpr uint32_t bytes_per_pixel = 4u;

    ScanoutResource(Platform &platform, ObjectStorage<VirtioGpuDevice> &device, uint32_t source);
    ScanoutResource(const ScanoutResource &) = delete;
    ScanoutResource &operator=(const ScanoutResource &) = delete;

    /* The resource the committed mode needs, made once per mode. */
    [[nodiscard]] Status configure(const DisplayMode &mode);
    /* The host stops showing this source; the resource is kept. */
    [[nodiscard]] Status disable();
    /* Tells the host to forget the resource. */
    void                 release();
    /* A new device holds none of the old one's resources. */
    void                 forget_device();
    /* The primary at `address`, and whether a frame was presented since the
     * last call: attaches and scans out an address the host has not been told,
     * and asks the host to read the pixels again when there is a new frame. */
    [[nodiscard]] Status refresh(uint64_t address, bool presented);

    bool invariants_hold() const;

private:
    [[nodiscard]] Status create(VirtioGpuDevice &device);
    [[nodiscard]] Status attach(VirtioGpuDevice &device, uint64_t address);
    [[nodiscard]] Status show(VirtioGpuDevice &device, uint32_t resource);
    [[nodiscard]] Status flush(VirtioGpuDevice &device);
    /* SET_SCANOUT 0 then RESOURCE_UNREF: the host holds nothing afterwards. */
    [[nodiscard]] Status unref();

    Platform                       &platform_;
    ObjectStorage<VirtioGpuDevice> &device_;
    uint32_t                        source_;
    uint32_t                        width_;
    uint32_t                        height_;
    uint64_t                        attached_address_;
    bool                            created_;
    bool                            scanning_out_;

    friend struct ScanoutResourceProbe;
};

}  // namespace av

#endif  // AV_SCANOUT_RESOURCE_HPP
