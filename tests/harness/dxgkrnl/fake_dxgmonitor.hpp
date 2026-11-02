#ifndef AV_FAKE_DXGMONITOR_HPP
#define AV_FAKE_DXGMONITOR_HPP

#include "miniport/driver/ddi_table.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeDxgmonitor {
public:
    static constexpr std::int32_t status_unsuccessful = static_cast<std::int32_t>(0xC0000001u);
    static constexpr std::int32_t status_not_supported = static_cast<std::int32_t>(0xC00000BBu);

    FakeDxgmonitor(MockPlatform &platform, const DdiTable &table, void *adapter, std::uint32_t driver_version);

    std::int32_t update_link_info(std::uint32_t target, std::uint32_t usage);

    std::uint32_t usage_hints() const;
    std::uint32_t capabilities() const;
    std::uint32_t dithering_support() const;

private:
    enum : std::uint32_t {
        hint_hidden = 0x1u,
        hint_head_mounted = 0x2u,
        caps_before_2_4 = 0x7Fu,
        caps_target_independent_primary = 0x80u,
        caps_variable_refresh = 0x800u,
        dithering_preference = 0x3u,
    };
    enum : std::uint32_t { driver_2_4 = 2400u, driver_2_6 = 2600u };

    std::int32_t filter(std::uint32_t capabilities, std::uint32_t dithering);

    MockPlatform   &platform_;
    const DdiTable &table_;
    void           *adapter_;
    std::uint32_t   driver_version_;
    std::uint32_t   usage_hints_;
    std::uint32_t   capabilities_;
    std::uint32_t   dithering_support_;
};

}

#endif
