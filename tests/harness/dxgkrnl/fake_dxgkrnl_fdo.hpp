#ifndef AV_FAKE_DXGKRNL_FDO_HPP
#define AV_FAKE_DXGKRNL_FDO_HPP

#include "miniport/driver/ddi_table.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class FakeDxgkrnlFdo {
public:
    static constexpr std::uint32_t max_children = 4u;
    static constexpr std::uint32_t max_links = 0x100u;

    FakeDxgkrnlFdo(MockPlatform &platform, const DdiTable &table, FakeDxgkrnlAdapter &adapter);

    using Stopping = void (*)(void *context);
    void         on_stopping(Stopping stopping, void *context);

    std::int32_t add_device();
    std::int32_t start_device();
    std::int32_t enumerate_children();
    std::uint8_t interrupt(std::uint32_t message);
    void         dpc();
    std::int32_t set_power_state(std::int32_t device_power_state);
    void         reset_device();
    std::int32_t stop_device();
    std::int32_t surprise_removal();
    std::int32_t remove_device();

    FakeDxgadapter &dxgadapter();
    void        *context() const;
    bool         started() const;
    std::uint32_t sources() const;
    std::uint32_t children() const;
    std::uint32_t connected_children() const;

private:
    Stopping      stopping_ = nullptr;
    void         *stopping_context_ = nullptr;

    MockPlatform       &platform_;
    const DdiTable     &table_;
    FakeDxgkrnlAdapter &adapter_;
    FakeDxgadapter      dxgadapter_;
    void               *context_;
    bool                started_;
    std::uint32_t       sources_;
    std::uint32_t       children_;
    std::uint32_t       connected_;
};

}

#endif
