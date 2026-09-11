#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/memory/root_page_table.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

using av::DxgkargSetRootPageTableRecord;
using av::GpuMmuGeometry;
using av::Irql;
using av::MockPlatform;
using av::RootPageTable;
using av::Status;
using av::Violation;

namespace av {

struct RootPageTableProbe {
    static void overfill(RootPageTable &root) { root.entries_ = GpuMmuGeometry::entries_per_table + 1u; }
};

}  // namespace av

namespace {

class Fixture {
public:
    Fixture() : geometry_(platform_), root_(platform_, geometry_) {}
    MockPlatform  &platform() { return platform_; }
    RootPageTable &root() { return root_; }

private:
    MockPlatform   platform_;
    GpuMmuGeometry geometry_;
    RootPageTable  root_;
};

DxgkargSetRootPageTableRecord root_of(std::uint32_t entries)
{
    return {nullptr, 2u, 0u, 0x5000u, entries, 0u};
}

}  // namespace

TEST(root_page_table_admits, any_address_before_a_root_is_assigned)
{ Fixture fixture;
  CHECK(fixture.root().admits(0xFFFFFFFFFFFFFFFFull));
  CHECK(fixture.root().invariants_hold()); }

TEST(root_page_table_assign, limits_submissions_to_the_entries_in_use)
{ Fixture fixture;
  CHECK_STATUS(fixture.root().assign(root_of(2u)), Status::ok);
  CHECK(fixture.root().invariants_hold());
  CHECK(fixture.root().admits(0xFFFFFFFFFFull));
  CHECK(!fixture.root().admits(0x10000000000ull));
  CHECK_STATUS(fixture.root().assign(root_of(512u)), Status::ok);
  CHECK(fixture.root().admits(0xFFFFFFFFFFFFull));
  CHECK(!fixture.root().admits(0x1000000000000ull)); }

TEST(root_page_table_assign, a_root_no_table_can_hold_keeps_the_old_one)
{ Fixture fixture;
  CHECK_STATUS(fixture.root().assign(root_of(1u)), Status::ok);
  CHECK_STATUS(fixture.root().assign(root_of(0u)), Status::invalid_parameter);
  CHECK_STATUS(fixture.root().assign(root_of(513u)), Status::invalid_parameter);
  CHECK(fixture.root().invariants_hold());
  CHECK(!fixture.root().admits(0x8000000000ull)); }

TEST(root_page_table_invariants, too_many_entries_is_detected)
{ Fixture fixture;
  av::RootPageTableProbe::overfill(fixture.root());
  CHECK(!fixture.root().invariants_hold()); }

TEST(root_page_table, assigns_at_passive_and_admits_at_dispatch)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.root().admits(0u));
  CHECK(!fixture.platform().claim(Violation::irql_ceiling));
  static_cast<void>(fixture.root().assign(root_of(1u)));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }
