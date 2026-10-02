/* dxgkrnl's display mode manager for one adapter, through the registered DDI
 * table at PASSIVE: it reads the monitor's EDID and modes, then sets a mode the
 * way a mode change does - propose the one-path VidPn, enumerate its
 * cofunctional modes, pin a source mode and enumerate again with the source as
 * pivot, pin the matching target mode, check the VidPn is still supported, and
 * commit it. */
#ifndef AV_FAKE_DISPLAY_MODE_MANAGER_HPP
#define AV_FAKE_DISPLAY_MODE_MANAGER_HPP

#include "miniport/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "miniport/ddi/dxgkarg_get_scan_line_record.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/vidpn/fake_vidpn.hpp"

#include <cstdint>

namespace av {

class FakeDisplayModeManager {
public:
    FakeDisplayModeManager(MockPlatform &platform, const DdiTable &table, void *adapter);
    FakeDisplayModeManager(const FakeDisplayModeManager &) = delete;
    FakeDisplayModeManager &operator=(const FakeDisplayModeManager &) = delete;

    /* QueryDeviceDescriptor for child 0, 128 bytes; the checksum byte sum must be zero. */
    std::int32_t read_edid(std::uint8_t *edid);
    /* RecommendMonitorModes into `vidpn`'s monitor source mode set. */
    std::int32_t read_monitor_modes(FakeVidPn &vidpn);
    /* Answers the first failure, or STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE
     * as a success status when a step leaves no mode of that size to pin. */
    std::int32_t set_mode(FakeVidPn &vidpn, std::uint32_t width, std::uint32_t height);
    std::int32_t is_supported(FakeVidPn &vidpn, bool &supported);
    std::int32_t commit(FakeVidPn &vidpn, std::uint32_t flags);
    std::int32_t update_active_path(const D3dkmdtVidPnPresentPathRecord &path);
    std::int32_t recommend_functional_vidpn();
    std::int32_t recommend_vidpn_topology();
    std::int32_t scan_line(DxgkargGetScanLineRecord &scan);
    std::int32_t control_vsync(bool enable);
    /* SetVidPnSourceAddress for source 0: the primary at `address` in segment 1, or none. */
    std::int32_t show_primary(void *allocation, std::int64_t address);
    /* VidSchiExecuteMmIoFlipAtISR @ 140038640: at device IRQL, with the segment
     * and address VidMm records for the primary and DXGK_SETVIDPNSOURCEADDRESS_FLAGS. */
    std::int32_t flip(void *allocation, std::uint32_t segment, std::int64_t address, std::uint32_t flags);
    std::int32_t set_visible(bool visible);

private:
    static constexpr std::int32_t no_pivot = 5;       /* D3DKMDT_EPT_NOPIVOT */
    static constexpr std::int32_t pivot_source = 1;   /* D3DKMDT_EPT_VIDPNSOURCE */

    std::int32_t enumerate(FakeVidPn &vidpn, std::int32_t pivot);

    MockPlatform   &platform_;
    const DdiTable &table_;
    void           *adapter_;
};

}  // namespace av

#endif  // AV_FAKE_DISPLAY_MODE_MANAGER_HPP
