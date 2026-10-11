/* One shared object the adapter knows. Its whole job is to count devices, and
 * the two ends of that count are what matter: a count that wrapped would make
 * an object nobody holds look held, and one that went below zero would free an
 * object another process still has open. */
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/memory/shared_object.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <cstdint>

namespace av {

struct SharedObjectProbe {
    static void set_opens(SharedObject &object, std::uint32_t opens) { object.opens_ = opens; }
    static void forget_allocation(SharedObject &object) { object.allocation_ = nullptr; }
    static void set_size(SharedObject &object, std::uint64_t size) { object.size_ = size; }
    static void set_id(SharedObject &object, std::uint64_t id) { object.id_ = id; }
    static void set_kind(SharedObject &object, std::uint32_t kind)
    { object.kind_ = SharedObjectKind(kind); }
    static std::uint32_t ceiling() { return SharedObject::max_opens; }
};

}  // namespace av

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::MockPlatform;
using av::ScanoutTarget;
using av::SharedObject;
using av::SharedObjectKind;
using av::SharedObjectProbe;
using av::Status;
using av::SurfaceLayout;

namespace {

constexpr std::uint64_t page = Allocation::page_bytes;

/* One allocation for the object to name, over the aperture this adapter has. */
class Fixture {
public:
    Fixture()
        : aperture_(platform_), segments_(platform_, aperture_),
          descriptor_(AllocationKind::client, false, SurfaceLayout(16u, 16u, 64u, 40u), 2u * page,
                      ScanoutTarget()),
          allocation_(platform_, segments_, descriptor_, 2u * page),
          object_(platform_, 7u, SharedObjectKind(SharedObjectKind::memory), 2u * page, allocation_)
    { CHECK_STATUS(aperture_.initialize(0x100000000), Status::ok); }

    MockPlatform &platform() { return platform_; }
    Allocation   &allocation() { return allocation_; }
    SharedObject &object() { return object_; }
    void          check()
    { if (!object_.invariants_hold())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "invariants broken"); }

private:
    MockPlatform            platform_;
    av::ApertureSegment     aperture_;
    av::MemorySegments      segments_;
    AllocationDescriptor    descriptor_;
    Allocation              allocation_;
    SharedObject            object_;
};

}  // namespace

TEST(shared_object, is_born_naming_its_allocation_with_nobody_holding_it)
{ Fixture rig;
  rig.check();
  CHECK_EQ(rig.object().id(), 7u);
  CHECK_EQ(rig.object().kind().value(), SharedObjectKind::memory);
  CHECK_EQ(rig.object().size(), 2u * page);
  CHECK_EQ(rig.object().opens(), 0u);
  CHECK(rig.object().names(rig.allocation())); }

TEST(shared_object_names, is_false_for_any_other_allocation)
{ Fixture rig;
  MockPlatform            other_platform;
  av::ApertureSegment     aperture(other_platform);
  av::MemorySegments      segments(other_platform, aperture);
  CHECK_STATUS(aperture.initialize(0x100000000), Status::ok);
  const AllocationDescriptor descriptor(AllocationKind::client, false, SurfaceLayout(8u, 8u, 32u, 40u),
                                        page, ScanoutTarget());
  Allocation other(other_platform, segments, descriptor, page);
  CHECK(!rig.object().names(other)); }

TEST(shared_object_open, counts_a_device_each_time_and_close_gives_one_back)
{ Fixture rig;
  CHECK_STATUS(rig.object().open(), Status::ok);
  rig.check();
  CHECK_EQ(rig.object().opens(), 1u);
  CHECK_STATUS(rig.object().open(), Status::ok);
  rig.check();
  CHECK_EQ(rig.object().opens(), 2u);
  CHECK_STATUS(rig.object().close(), Status::ok);
  rig.check();
  CHECK_EQ(rig.object().opens(), 1u);
  CHECK_STATUS(rig.object().close(), Status::ok);
  rig.check();
  CHECK_EQ(rig.object().opens(), 0u); }

TEST(shared_object_close, refuses_when_nobody_holds_it_and_leaves_the_count_alone)
{ Fixture rig;
  CHECK_STATUS(rig.object().close(), Status::invalid_parameter);
  rig.check();
  CHECK_EQ(rig.object().opens(), 0u);
  CHECK_STATUS(rig.object().open(), Status::ok);
  CHECK_STATUS(rig.object().close(), Status::ok);
  CHECK_STATUS(rig.object().close(), Status::invalid_parameter);
  rig.check();
  CHECK_EQ(rig.object().opens(), 0u); }

TEST(shared_object_open, refuses_at_the_ceiling_rather_than_wrapping_to_nobody_holding_it)
{ Fixture rig;
  SharedObjectProbe::set_opens(rig.object(), SharedObjectProbe::ceiling());
  rig.check();
  CHECK_STATUS(rig.object().open(), Status::no_space);
  rig.check();
  CHECK_EQ(rig.object().opens(), SharedObjectProbe::ceiling());
  /* And one below it still takes the last one. */
  SharedObjectProbe::set_opens(rig.object(), SharedObjectProbe::ceiling() - 1u);
  CHECK_STATUS(rig.object().open(), Status::ok);
  rig.check();
  CHECK_EQ(rig.object().opens(), SharedObjectProbe::ceiling());
  SharedObjectProbe::set_opens(rig.object(), 0u); }

TEST(shared_object, open_and_close_are_legal_up_to_apc_because_the_table_holds_a_mutex)
{ Fixture rig;
  rig.platform().set_irql(av::Irql::apc);
  CHECK_STATUS(rig.object().open(), Status::ok);
  CHECK_STATUS(rig.object().close(), Status::ok);
  rig.platform().set_irql(av::Irql::passive);
  rig.check(); }

TEST(shared_object_invariants, refuse_an_object_with_no_id_no_kind_no_allocation_or_a_partial_page)
{ Fixture rig;
  CHECK(rig.object().invariants_hold());

  SharedObjectProbe::set_id(rig.object(), 0u);
  CHECK(!rig.object().invariants_hold());
  SharedObjectProbe::set_id(rig.object(), 7u);
  CHECK(rig.object().invariants_hold());

  SharedObjectProbe::set_kind(rig.object(), 0u);
  CHECK(!rig.object().invariants_hold());
  SharedObjectProbe::set_kind(rig.object(), SharedObjectKind::native_fence + 1u);
  CHECK(!rig.object().invariants_hold());
  SharedObjectProbe::set_kind(rig.object(), SharedObjectKind::memory);
  CHECK(rig.object().invariants_hold());

  SharedObjectProbe::set_size(rig.object(), 0u);
  CHECK(!rig.object().invariants_hold());
  SharedObjectProbe::set_size(rig.object(), page + 1u);
  CHECK(!rig.object().invariants_hold());
  SharedObjectProbe::set_size(rig.object(), 2u * page);
  CHECK(rig.object().invariants_hold());

  /* A fence's size is none at all, which is the record's rule and the wire
   * body's: an object of either kind holding pages is one no create made. */
  SharedObjectProbe::set_kind(rig.object(), SharedObjectKind::monitored_fence);
  CHECK(!rig.object().invariants_hold());
  SharedObjectProbe::set_size(rig.object(), 0u);
  CHECK(rig.object().invariants_hold());
  SharedObjectProbe::set_kind(rig.object(), SharedObjectKind::native_fence);
  CHECK(rig.object().invariants_hold());
  SharedObjectProbe::set_kind(rig.object(), SharedObjectKind::memory);
  CHECK(!rig.object().invariants_hold());
  SharedObjectProbe::set_size(rig.object(), 2u * page);
  CHECK(rig.object().invariants_hold());

  /* Forgotten while still held is a count that lost track: dxgkrnl destroys
   * a shared allocation only once nobody holds it. */
  CHECK_STATUS(rig.object().open(), Status::ok);
  SharedObjectProbe::forget_allocation(rig.object());
  CHECK(!rig.object().invariants_hold());
  CHECK_STATUS(rig.object().close(), Status::ok);
  CHECK(rig.object().invariants_hold()); }

/* Forgotten as its allocation is destroyed: it names that allocation no more,
 * and says so. */
TEST(shared_object, a_forgotten_object_names_no_allocation)
{ Fixture rig;
  CHECK(!rig.object().forgotten());
  rig.object().forget();
  CHECK(rig.object().forgotten());
  CHECK(rig.object().invariants_hold()); }
