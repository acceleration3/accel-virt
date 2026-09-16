/* What DriverEntry does, kept out of the shim so it has no branch: construct the
 * platform, start the trace table, construct the miniport root in storage that
 * outlives DriverEntry, bind the root for the context-less DDIs, log, and
 * register the DDI table.
 *
 * A template over the platform and the trace host because the real ones are
 * constructed differently and live in different storage: KernelPlatform and
 * KernelTraceHost in the shim's static storage (src/km/driver_entry.cpp),
 * MockPlatform and MockTraceHost in tests. */
#ifndef AV_DRIVER_IMAGE_HPP
#define AV_DRIVER_IMAGE_HPP

#include "core/ddi/entry_points.hpp"
#include "core/miniport/driver_trace.hpp"
#include "core/miniport/miniport.hpp"
#include "core/support/nt_status.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

template <typename PlatformType, typename TraceHostType>
struct DriverImageProbe;

template <typename PlatformType, typename TraceHostType>
class DriverImage {
public:
    DriverImage() = default;
    DriverImage(const DriverImage &) = delete;
    DriverImage &operator=(const DriverImage &) = delete;

    /* Returns the NTSTATUS DriverEntry returns. `image` is the driver's own
     * address range; `bugcheck` is what the kernel calls if it bugchecks, and
     * calls bugcheck() below. */
    template <typename... PlatformArguments>
    int32_t enter(void *driver_object, void *registry_path, const ImageRange &image, BugcheckRoutine bugcheck,
                  PlatformArguments &&...arguments)
    {
        AV_SCOPE("DriverImage::enter");
        if (platform_.construct(static_cast<PlatformArguments &&>(arguments)...) != Status::ok)
            return NtStatus(Status::invalid_state).value();
        PlatformType &platform = platform_.object();
        platform.log("rvkgpu: DriverEntry");
        /* Both storages are empty whenever the platform's was. */
        static_cast<void>(host_.construct());
        static_cast<void>(trace_.construct(platform, host_.object(), image));
        trace_.object().start(bugcheck);
        if (miniport_.construct(platform) != Status::ok) {
            leave_trace();
            platform_.destroy();
            return NtStatus(Status::invalid_state).value();
        }
        miniport_binding.bind(miniport_.object());
        const int32_t registered =
            platform.register_miniport(driver_object, registry_path, miniport_.object().ddi_table());
        if (registered < 0) {
            char       bytes[80];
            TextBuffer message(bytes, sizeof bytes);
            message.append("rvkgpu: DxgkInitialize refused the driver, status 0x");
            message.append_hex(static_cast<uint32_t>(registered), 8u);
            platform.log(message.text());
            leave();
        }
        return registered;
    }

    /* After DxgkDdiUnload, or when registration failed. The table outlives the
     * miniport, whose teardown still opens scopes. */
    void leave()
    {
        AV_SCOPE("DriverImage::leave");
        miniport_binding.unbind();
        miniport_.destroy();
        leave_trace();
        platform_.destroy();
    }

    /* The kernel is bugchecking: the table, if there is one, to the debug port. */
    void bugcheck()
    {
        AV_SCOPE("DriverImage::bugcheck");
        if (trace_.constructed()) trace_.object().bugcheck();
    }

    PlatformType &platform()
    {
        AV_SCOPE("DriverImage::platform");
        return platform_.object();
    }

private:
    void leave_trace()
    {
        AV_SCOPE("DriverImage::leave_trace");
        if (trace_.constructed()) trace_.object().stop();
        trace_.destroy();
        host_.destroy();
    }

    ObjectStorage<PlatformType>  platform_;
    ObjectStorage<TraceHostType> host_;
    ObjectStorage<DriverTrace>   trace_;
    ObjectStorage<Miniport>      miniport_;

    friend struct DriverImageProbe<PlatformType, TraceHostType>;
};

}  // namespace av

#endif  // AV_DRIVER_IMAGE_HPP
