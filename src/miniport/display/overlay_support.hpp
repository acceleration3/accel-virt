#ifndef AV_OVERLAY_SUPPORT_HPP
#define AV_OVERLAY_SUPPORT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class CapsetInfo;
class DisplaySources;
struct DxgkargCheckMultiPlaneOverlaySupport3Record;
struct DxgkargGetMultiPlaneOverlayCapsRecord;
struct DxgkargGetPostCompositionCapsRecord;
struct DxgkMultiplaneOverlayPlaneWithSource2Record;
struct DxgkMultiplaneOverlayPostCompositionWithSourceRecord;

class OverlaySupport {
public:
    static constexpr uint32_t max_scale = 8u;

    OverlaySupport(Platform &platform, DisplaySources &sources);
    OverlaySupport(const OverlaySupport &) = delete;
    OverlaySupport &operator=(const OverlaySupport &) = delete;

    void offer(const CapsetInfo &capset);
    uint32_t planes() const;

    [[nodiscard]] Status caps(DxgkargGetMultiPlaneOverlayCapsRecord &caps) const;
    [[nodiscard]] Status post_composition_caps(DxgkargGetPostCompositionCapsRecord &caps) const;
    [[nodiscard]] Status check(DxgkargCheckMultiPlaneOverlaySupport3Record &check) const;

private:
    bool shows(const DxgkMultiplaneOverlayPlaneWithSource2Record &plane) const;
    bool shows(const DxgkMultiplaneOverlayPostCompositionWithSourceRecord &composition) const;

    Platform       &platform_;
    DisplaySources &sources_;
    bool            composed_;
};

}

#endif
