/* The native GPU fences dxgkrnl makes on this adapter (WDDM 3.1, feature 049).
 *
 * The OS owns the memory a fence's two values live in and hands the driver the
 * address each time it wants one written, so what these tests judge is what
 * the driver owns: which fences exist, that a handle it never gave is refused
 * rather than followed, and that a request naming one bad fence writes nothing
 * at all - a half-written update would leave the OS's waiters reading a
 * mixture of what it asked for and what it did not.
 */
#include "miniport/ddi/dxgkarg_native_fence_records.hpp"
#include "miniport/scheduling/native_fence_table.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::DxgkargCreateNativeFenceRecord;
using av::DxgkargDestroyNativeFenceRecord;
using av::DxgkargUpdateNativeFenceValuesRecord;
using av::Irql;
using av::MockPlatform;
using av::NativeFenceTable;
using av::Status;
using av::Violation;

namespace {

/* One fence and the two values the OS keeps for it. */
class Fence {
public:
    Fence(NativeFenceTable &table, std::uint32_t type = av::native_fence_type_default)
    {
        DxgkargCreateNativeFenceRecord request = {};
        request.Type = type;
        CHECK_STATUS(table.create(request), Status::ok);
        handle_ = request.hGlobalNativeFence;
        CHECK(handle_ != nullptr);
    }

    void         *handle() const { return handle_; }
    std::uint64_t current = 0u;
    std::uint64_t monitored = 0u;

private:
    void *handle_ = nullptr;
};

/* What the OS passes: which fences, what values, and where to write them. */
class Update {
public:
    Update(void *const *handles, const std::uint64_t *values, void *const *addresses, std::uint32_t count)
    {
        for (std::uint32_t index = 0u; index < count; index++) {
            handles_[index] = handles[index];
            values_[index] = values[index];
            addresses_[index] = addresses[index];
        }
        record_.NativeFenceArray = handles_;
        record_.UpdatedValueArray = values_;
        record_.ValueKernelCpuVa = addresses_;
        record_.NumFences = count;
    }

    DxgkargUpdateNativeFenceValuesRecord &record() { return record_; }

private:
    void                                *handles_[8] = {};
    std::uint64_t                        values_[8] = {};
    void                                *addresses_[8] = {};
    DxgkargUpdateNativeFenceValuesRecord record_ = {};
};

}  // namespace

TEST(native_fence_table, writes_what_the_os_asks_where_the_os_says)
{ MockPlatform     platform;
  NativeFenceTable fences(platform);
  CHECK(fences.invariants_hold());
  CHECK_EQ(fences.live(), 0u);
  Fence first(fences);
  Fence second(fences, av::native_fence_type_intra_gpu);
  CHECK_EQ(fences.live(), 2u);
  CHECK(first.handle() != second.handle());
  CHECK(fences.invariants_hold());

  void *const         handles[] = {first.handle(), second.handle()};
  const std::uint64_t values[] = {7u, 9u};
  void *const         monitored[] = {&first.monitored, &second.monitored};
  void *const         current[] = {&first.current, &second.current};
  Update              monitored_update(handles, values, monitored, 2u);
  Update              current_update(handles, values, current, 2u);
  CHECK_STATUS(fences.update_monitored(monitored_update.record()), Status::ok);
  CHECK_EQ(first.monitored, 7u);
  CHECK_EQ(second.monitored, 9u);
  CHECK_EQ(first.current, 0u);
  CHECK_EQ(fences.monitored_value(first.handle()), 7u);
  CHECK_EQ(fences.monitored_value(second.handle()), 9u);
  CHECK_STATUS(fences.update_current(current_update.record()), Status::ok);
  CHECK_EQ(first.current, 7u);
  CHECK_EQ(fences.current_value(first.handle()), 7u);
  CHECK_EQ(fences.current_value(second.handle()), 9u);
  CHECK(fences.invariants_hold());

  /* Destroyed, a fence is one this driver no longer has: nothing of it is
   * remembered and nothing is written for it again. */
  const DxgkargDestroyNativeFenceRecord destroy = {first.handle(), 0u, {}};
  CHECK_STATUS(fences.destroy(destroy), Status::ok);
  CHECK_EQ(fences.live(), 1u);
  CHECK_EQ(fences.current_value(first.handle()), 0u);
  CHECK_EQ(fences.monitored_value(first.handle()), 0u);
  CHECK_STATUS(fences.destroy(destroy), Status::invalid_parameter);
  CHECK(fences.invariants_hold()); }

/* Everything the OS could pass that names no fence of this driver's, and a
 * request with one bad entry among good ones - which writes nothing. */
TEST(native_fence_table, a_request_naming_anything_it_did_not_make_writes_nothing)
{ MockPlatform     platform;
  NativeFenceTable fences(platform);
  Fence            fence(fences);
  int              outside = 0;

  DxgkargCreateNativeFenceRecord odd = {};
  odd.Type = 7u;
  CHECK_STATUS(fences.create(odd), Status::invalid_parameter);
  CHECK(odd.hGlobalNativeFence == nullptr);
  const DxgkargDestroyNativeFenceRecord stranger = {&outside, 0u, {}};
  CHECK_STATUS(fences.destroy(stranger), Status::invalid_parameter);
  const DxgkargDestroyNativeFenceRecord none = {nullptr, 0u, {}};
  CHECK_STATUS(fences.destroy(none), Status::invalid_parameter);

  void *const         handles[] = {fence.handle(), &outside};
  const std::uint64_t values[] = {3u, 4u};
  void *const         addresses[] = {&fence.current, &fence.monitored};
  Update              mixed(handles, values, addresses, 2u);
  CHECK_STATUS(fences.update_current(mixed.record()), Status::invalid_parameter);
  CHECK_EQ(fence.current, 0u);
  CHECK_EQ(fence.monitored, 0u);
  CHECK_STATUS(fences.update_monitored(mixed.record()), Status::invalid_parameter);
  CHECK_EQ(fence.monitored, 0u);

  /* And every shape of request that is no request. */
  void *const         one[] = {fence.handle()};
  const std::uint64_t value[] = {5u};
  void *const         address[] = {&fence.current};
  Update              good(one, value, address, 1u);
  DxgkargUpdateNativeFenceValuesRecord empty = good.record();
  empty.NumFences = 0u;
  CHECK_STATUS(fences.update_current(empty), Status::invalid_parameter);
  DxgkargUpdateNativeFenceValuesRecord too_many = good.record();
  too_many.NumFences = NativeFenceTable::max_fences + 1u;
  CHECK_STATUS(fences.update_current(too_many), Status::invalid_parameter);
  DxgkargUpdateNativeFenceValuesRecord no_fences = good.record();
  no_fences.NativeFenceArray = nullptr;
  CHECK_STATUS(fences.update_current(no_fences), Status::invalid_parameter);
  DxgkargUpdateNativeFenceValuesRecord no_values = good.record();
  no_values.UpdatedValueArray = nullptr;
  CHECK_STATUS(fences.update_current(no_values), Status::invalid_parameter);
  DxgkargUpdateNativeFenceValuesRecord no_addresses = good.record();
  no_addresses.ValueKernelCpuVa = nullptr;
  CHECK_STATUS(fences.update_current(no_addresses), Status::invalid_parameter);
  void *const         nowhere[] = {nullptr};
  Update              unaddressed(one, value, nowhere, 1u);
  CHECK_STATUS(fences.update_current(unaddressed.record()), Status::invalid_parameter);
  CHECK_EQ(fence.current, 0u);
  /* The good one still writes. */
  CHECK_STATUS(fences.update_current(good.record()), Status::ok);
  CHECK_EQ(fence.current, 5u);
  CHECK(fences.invariants_hold()); }

/* A table with no room refuses rather than handing out a fence nobody tracks. */
TEST(native_fence_table, a_table_with_no_room_refuses)
{ MockPlatform     platform;
  NativeFenceTable fences(platform);
  void            *last = nullptr;
  for (std::uint32_t made = 0u; made < NativeFenceTable::max_fences; made++) {
      DxgkargCreateNativeFenceRecord request = {};
      CHECK_STATUS(fences.create(request), Status::ok);
      last = request.hGlobalNativeFence;
  }
  CHECK_EQ(fences.live(), NativeFenceTable::max_fences);
  DxgkargCreateNativeFenceRecord past = {};
  CHECK_STATUS(fences.create(past), Status::no_space);
  CHECK(past.hGlobalNativeFence == nullptr);
  CHECK(fences.invariants_hold());
  const DxgkargDestroyNativeFenceRecord destroy = {last, 0u, {}};
  CHECK_STATUS(fences.destroy(destroy), Status::ok);
  CHECK_STATUS(fences.create(past), Status::ok); }

/* What av-inspect reads out of a running driver (docs/trace.md), and nothing
 * but `busy` while another thread holds the table. */
TEST(native_fence_table_report, writes_each_fence_and_says_when_it_cannot)
{ MockPlatform     platform;
  NativeFenceTable fences(platform);
  Fence            fence(fences);
  void *const         one[] = {fence.handle()};
  const std::uint64_t value[] = {12u};
  void *const         address[] = {&fence.current};
  Update              update(one, value, address, 1u);
  CHECK_STATUS(fences.update_current(update.record()), Status::ok);
  char            bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  fences.report(writer);
  CHECK(std::strcmp(writer.dump(),
                    "native_fence_table/0 busy=false live=1\n"
                    "native_fence_table/0/native_fence/0 type=0 current=12 monitored=0") == 0);

  platform.hold_elsewhere(av::LockRank::native_fence_table);
  av::StateWriter busy(bytes, sizeof bytes);
  fences.report(busy);
  CHECK(std::strcmp(busy.dump(), "native_fence_table/0 busy=true") == 0);
  platform.released_elsewhere(); }

TEST(native_fence_table, runs_at_the_levels_it_declares)
{ MockPlatform     platform;
  NativeFenceTable fences(platform);
  Fence            fence(fences);
  void *const         one[] = {fence.handle()};
  const std::uint64_t value[] = {1u};
  void *const         address[] = {&fence.current};
  Update              update(one, value, address, 1u);

  /* The updates are the OS's own: monitored values arrive as high as PROFILE,
   * and the ones from the CPU at DISPATCH. */
  platform.set_irql(Irql::isr);
  CHECK_STATUS(fences.update_monitored(update.record()), Status::ok);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(fences.update_current(update.record()), Status::ok);
  static_cast<void>(fences.live());
  static_cast<void>(fences.current_value(fence.handle()));
  static_cast<void>(fences.monitored_value(fence.handle()));
  CHECK_EQ(platform.unclaimed_violations(), 0u);

  /* Creating and destroying one is PASSIVE, and the report is APC. */
  DxgkargCreateNativeFenceRecord request = {};
  static_cast<void>(fences.create(request));
  const DxgkargDestroyNativeFenceRecord destroy = {fence.handle(), 0u, {}};
  static_cast<void>(fences.destroy(destroy));
  char            bytes[128];
  av::StateWriter writer(bytes, sizeof bytes);
  fences.report(writer);
  platform.set_irql(Irql::passive);
  while (platform.claim(Violation::irql_ceiling)) {}
  while (platform.claim(Violation::blocking_at_dispatch)) {}
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

namespace av {

struct NativeFenceTableProbe {
    static void leave_behind(NativeFenceTable &fences, std::uint32_t slot, std::uint64_t current,
                             std::uint64_t monitored)
    {
        fences.current_[slot] = current;
        fences.monitored_[slot] = monitored;
    }
};

}  // namespace av

/* A slot nobody holds holds nothing: a value left in a destroyed fence would
 * be read by the next process handed that slot. */
TEST(native_fence_table_invariants, a_value_left_in_a_dead_slot_is_caught)
{ MockPlatform     platform;
  NativeFenceTable fences(platform);
  CHECK(fences.invariants_hold());
  av::NativeFenceTableProbe::leave_behind(fences, 3u, 7u, 0u);
  CHECK(!fences.invariants_hold());
  av::NativeFenceTableProbe::leave_behind(fences, 3u, 0u, 0u);
  CHECK(fences.invariants_hold());
  av::NativeFenceTableProbe::leave_behind(fences, 5u, 0u, 9u);
  CHECK(!fences.invariants_hold()); }
