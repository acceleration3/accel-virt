/* What DriverEntry does, kept out of the shim so it has no branch: construct the
 * platform and the miniport root in storage that outlives DriverEntry, bind the
 * root for the context-less DDIs, log, and register the DDI table.
 *
 * A template over the platform because the two real platforms are constructed
 * differently and live in different storage: KernelPlatform in the shim's
 * static storage (src/km/driver_entry.cpp), MockPlatform in tests. */
#ifndef AV_DRIVER_IMAGE_HPP
#define AV_DRIVER_IMAGE_HPP

#include "ddi/entry_points.hpp"
#include "miniport.hpp"
#include "nt_status.hpp"
#include "object_storage.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

template <typename PlatformType>
struct DriverImageProbe;

template <typename PlatformType>
class DriverImage {
public:
    DriverImage() = default;
    DriverImage(const DriverImage &) = delete;
    DriverImage &operator=(const DriverImage &) = delete;

    /* Returns the NTSTATUS DriverEntry returns. */
    template <typename... PlatformArguments>
    int32_t enter(void *driver_object, void *registry_path, PlatformArguments &&...arguments)
    {
        if (platform_.construct(static_cast<PlatformArguments &&>(arguments)...) != Status::ok)
            return NtStatus(Status::invalid_state).value();
        PlatformType &platform = platform_.object();
        platform.log("rvkgpu: DriverEntry");
        if (miniport_.construct(platform) != Status::ok) {
            platform_.destroy();
            return NtStatus(Status::invalid_state).value();
        }
        miniport_binding.bind(miniport_.object());
        const int32_t registered =
            platform.register_miniport(driver_object, registry_path, miniport_.object().ddi_table());
        if (registered < 0) leave();
        return registered;
    }

    /* After DxgkDdiUnload, or when registration failed. */
    void leave()
    {
        miniport_binding.unbind();
        miniport_.destroy();
        platform_.destroy();
    }

    PlatformType &platform() { return platform_.object(); }

private:
    ObjectStorage<PlatformType> platform_;
    ObjectStorage<Miniport>     miniport_;

    friend struct DriverImageProbe<PlatformType>;
};

}  // namespace av

#endif  // AV_DRIVER_IMAGE_HPP
