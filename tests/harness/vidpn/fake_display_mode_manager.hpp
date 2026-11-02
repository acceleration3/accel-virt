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

    std::int32_t read_edid(std::uint8_t *edid);
    std::int32_t read_monitor_modes(FakeVidPn &vidpn);
    std::int32_t set_mode(FakeVidPn &vidpn, std::uint32_t width, std::uint32_t height);
    std::int32_t is_supported(FakeVidPn &vidpn, bool &supported);
    std::int32_t commit(FakeVidPn &vidpn, std::uint32_t flags);
    std::int32_t update_active_path(const D3dkmdtVidPnPresentPathRecord &path);
    std::int32_t recommend_functional_vidpn();
    std::int32_t recommend_vidpn_topology();
    std::int32_t scan_line(DxgkargGetScanLineRecord &scan);
    std::int32_t control_vsync(bool enable);
    std::int32_t show_primary(void *allocation, std::int64_t address);
    std::int32_t flip(void *allocation, std::uint32_t segment, std::int64_t address, std::uint32_t flags);
    std::int32_t set_visible(bool visible);

private:
    static constexpr std::int32_t no_pivot = 5;
    static constexpr std::int32_t pivot_source = 1;

    std::int32_t enumerate(FakeVidPn &vidpn, std::int32_t pivot);

    MockPlatform   &platform_;
    const DdiTable &table_;
    void           *adapter_;
};

}

#endif
