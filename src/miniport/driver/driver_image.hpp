#ifndef AV_DRIVER_IMAGE_HPP
#define AV_DRIVER_IMAGE_HPP

#include "miniport/ddi/entry_points.hpp"
#include "miniport/driver/driver_trace.hpp"
#include "miniport/driver/image_exit.hpp"
#include "miniport/driver/miniport.hpp"
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
class DriverImage final : public ImageExit {
public:
    DriverImage() = default;
    DriverImage(const DriverImage &) = delete;
    DriverImage &operator=(const DriverImage &) = delete;

    template <typename... PlatformArguments>
    int32_t enter(void *driver_object, void *registry_path, const ImageRange &image, BugcheckRoutine bugcheck,
                  PlatformArguments &&...arguments)
    {
        AV_SCOPE("DriverImage::enter");
        if (platform_.construct(static_cast<PlatformArguments &&>(arguments)...) != Status::ok)
            return NtStatus(Status::invalid_state).value();
        PlatformType &platform = platform_.object();
        platform.log("avgpu: DriverEntry");
        static_cast<void>(host_.construct());
        static_cast<void>(trace_.construct(platform, host_.object(), image));
        if (platform.read_service_u32(registry_path, "TraceScopes", 0u) != 0u) trace_.object().start(bugcheck);
        if (miniport_.construct(platform) != Status::ok) {
            leave_trace();
            platform_.destroy();
            return NtStatus(Status::invalid_state).value();
        }
        miniport_binding.bind(miniport_.object(), this);
        const int32_t registered =
            platform.register_miniport(driver_object, registry_path, miniport_.object().ddi_table());
        if (registered < 0) {
            char       bytes[80];
            TextBuffer message(bytes, sizeof bytes);
            message.append("avgpu: DxgkInitialize refused the driver, status 0x");
            message.append_hex(static_cast<uint32_t>(registered), 8u);
            platform.log(message.text());
            leave();
        }
        return registered;
    }

    void leave() override
    {
        AV_SCOPE("DriverImage::leave");
        miniport_binding.unbind();
        miniport_.destroy();
        leave_trace();
        platform_.destroy();
    }

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

}

#endif
