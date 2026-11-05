#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_allocation_data.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>
#include <cstring>

using av::AllocationDescriptor;
using av::SharedAllocationData;
using av::SharedObjectKind;
using av::SharedObjectRecord;
using av::Status;

namespace {

SharedAllocationData made()
{
    return SharedAllocationData(AllocationDescriptor(av::AllocationKind::surface, false,
                                                     av::SurfaceLayout(16u, 16u, 87u, 64u), 0x4000u, av::ScanoutTarget()),
                                SharedObjectKind(SharedObjectKind::memory), 0x4000u);
}

}

TEST(shared_allocation_data, is_unread_and_holds_nothing_until_it_is_given_a_descriptor_and_a_record)
{ SharedAllocationData data;
  CHECK(!data.read());
  CHECK(!data.assigned());
  CHECK_EQ(data.object_id(), 0u);
  CHECK_EQ(data.size(), 0u);
  CHECK(data.invariants_hold());
  const SharedAllocationData given = made();
  CHECK(given.read());
  CHECK(!given.assigned());
  CHECK_EQ(given.record().size(), 0x4000u);
  CHECK_EQ(given.size(), 0x4000u);
  CHECK(given.invariants_hold()); }

TEST(shared_allocation_data, mints_a_record_into_the_bytes_the_miniport_hands_back)
{ SharedAllocationData data = made();
  const SharedObjectRecord minted(77u, SharedObjectKind(SharedObjectKind::memory), 5u, 0x4000u);
  CHECK_STATUS(data.mint(minted), Status::ok);
  CHECK(data.assigned());
  CHECK_EQ(data.object_id(), 77u);
  CHECK(data.invariants_hold());
  SharedAllocationData again;
  CHECK_STATUS(again.decode(data.bytes(), SharedAllocationData::encoded_size), Status::ok);
  CHECK_EQ(again.object_id(), 77u);
  CHECK(again.read());
  SharedAllocationData unread;
  CHECK_STATUS(unread.mint(minted), Status::invalid_state); }

TEST(shared_allocation_data, rereads_what_was_written_through_the_lent_bytes_and_refuses_a_wrong_length_or_damage)
{ const SharedAllocationData source = made();
  SharedAllocationData       data;
  std::memcpy(data.lend(), source.bytes(), SharedAllocationData::encoded_size);
  CHECK(!data.read());
  CHECK_STATUS(data.reread(SharedAllocationData::encoded_size - 1u), Status::malformed);
  CHECK(!data.read());
  CHECK_STATUS(data.reread(SharedAllocationData::encoded_size), Status::ok);
  CHECK(data.read());
  CHECK(data.invariants_hold());
  data.lend()[0] ^= 0xFFu;
  CHECK_STATUS(data.reread(SharedAllocationData::encoded_size), Status::malformed);
  CHECK(!data.read());
  CHECK_STATUS(data.decode(nullptr, SharedAllocationData::encoded_size), Status::malformed);
  CHECK_STATUS(data.decode(source.bytes(), SharedAllocationData::encoded_size + 1u), Status::malformed); }
