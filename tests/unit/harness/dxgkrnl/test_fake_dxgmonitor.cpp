/* The harness's monitor link information handling, proven against crafted
 * answers so a driver claiming more than dxgkrnl trusts is caught. */
#include "miniport/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/fake_dxgmonitor.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::DdiRoutine;
using av::DdiSlot;
using av::DdiTable;
using av::DxgkargUpdateMonitorLinkInfoRecord;
using av::FakeDxgmonitor;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);

/* A miniport whose UpdateMonitorLinkInfo answers what the test set. */
class LinkMiniport {
public:
    LinkMiniport() : status_(0), capabilities_(0u), dithering_(0u), target_(0xFFu), hints_(0xFFu)
    {
        if (table_.set(DdiSlot::DxgkDdiUpdateMonitorLinkInfo, reinterpret_cast<DdiRoutine>(&LinkMiniport::answer)) !=
            Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "set failed");
    }
    void answer_with(std::int32_t status, std::uint32_t capabilities, std::uint32_t dithering)
    {
        status_ = status;
        capabilities_ = capabilities;
        dithering_ = dithering;
    }
    const DdiTable &table() const { return table_; }
    std::uint32_t   target() const { return target_; }
    std::uint32_t   hints() const { return hints_; }

    static std::int32_t answer(void *context, DxgkargUpdateMonitorLinkInfoRecord *request)
    {
        auto *self = static_cast<LinkMiniport *>(context);
        self->target_ = request->VideoPresentTargetId;
        self->hints_ = request->MonitorLinkInfoUsageHints;
        request->MonitorLinkInfoCapabilities = self->capabilities_;
        request->MonitorLinkInfoDitheringSupport = self->dithering_;
        return self->status_;
    }

private:
    DdiTable      table_;
    std::int32_t  status_;
    std::uint32_t capabilities_;
    std::uint32_t dithering_;
    std::uint32_t target_;
    std::uint32_t hints_;
};

}  // namespace

TEST(fake_dxgmonitor_update_link_info, sends_the_usage_as_hints_and_keeps_trusted_capabilities)
{ MockPlatform platform; LinkMiniport miniport;
  FakeDxgmonitor monitor(platform, miniport.table(), &miniport, 2100u);
  miniport.answer_with(0, 0x7Fu, 0x4u);
  CHECK_EQ(monitor.update_link_info(3u, 0u), 0);
  CHECK_EQ(miniport.target(), 3u);
  CHECK_EQ(miniport.hints(), 0u);
  CHECK_EQ(monitor.capabilities(), 0x7Fu);
  CHECK_EQ(monitor.dithering_support(), 0x4u);
  CHECK_EQ(monitor.update_link_info(0u, 1u), 0);
  CHECK_EQ(miniport.hints(), 0x3u);
  CHECK_EQ(monitor.usage_hints(), 0x3u);
  CHECK_EQ(monitor.update_link_info(0u, 2u), 0);
  CHECK_EQ(miniport.hints(), 0x1u); }

TEST(fake_dxgmonitor_update_link_info, below_2_4_untrusted_bits_are_masked_and_failures_swallowed)
{ MockPlatform platform; LinkMiniport miniport;
  FakeDxgmonitor monitor(platform, miniport.table(), &miniport, 2100u);
  miniport.answer_with(0, 0xFFFu, 0x5u);
  CHECK_EQ(monitor.update_link_info(0u, 0u), 0);
  CHECK_EQ(monitor.capabilities(), 0x7Fu);
  CHECK_EQ(monitor.dithering_support(), 0u);   /* a preference is not the driver's to give */
  miniport.answer_with(no_memory, 0x1u, 0u);
  CHECK_EQ(monitor.update_link_info(0u, 0u), 0);
  CHECK_EQ(monitor.capabilities(), 0u); }

TEST(fake_dxgmonitor_update_link_info, from_2_4_untrusted_bits_and_failures_reach_the_caller)
{ MockPlatform platform; LinkMiniport miniport;
  FakeDxgmonitor monitor(platform, miniport.table(), &miniport, 2400u);
  miniport.answer_with(0, 0x80u | 0x1u, 0u);
  CHECK_EQ(monitor.update_link_info(0u, 0u), 0);
  CHECK_EQ(monitor.capabilities(), 0x1u);      /* TargetIndependentPrimary is always cleared */
  miniport.answer_with(0, 0x800u, 0u);
  CHECK_EQ(monitor.update_link_info(0u, 0u), FakeDxgmonitor::status_unsuccessful);
  CHECK_EQ(monitor.capabilities(), 0u);
  FakeDxgmonitor later(platform, miniport.table(), &miniport, 2600u);
  CHECK_EQ(later.update_link_info(0u, 0u), 0);
  CHECK_EQ(later.capabilities(), 0x800u);
  miniport.answer_with(no_memory, 0u, 0u);
  CHECK_EQ(later.update_link_info(0u, 0u), no_memory); }

TEST(fake_dxgmonitor_update_link_info, a_null_slot_is_the_default_that_declines)
{ MockPlatform platform; const DdiTable table;
  FakeDxgmonitor old_driver(platform, table, nullptr, 2100u);
  CHECK_EQ(old_driver.update_link_info(0u, 0u), 0);
  FakeDxgmonitor new_driver(platform, table, nullptr, 2400u);
  CHECK_EQ(new_driver.update_link_info(0u, 0u), FakeDxgmonitor::status_not_supported); }
