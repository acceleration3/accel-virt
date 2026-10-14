/* The ids host blobs take: exactly `count` of them from `first`, so the
 * device indexes its table by them and refuses every other id. */
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdint>

using av::HostBlobId;

TEST(host_blob_id, the_range_is_valid_from_its_first_to_its_last_and_each_is_its_own_slot)
{ CHECK(HostBlobId(HostBlobId::first).valid());
  CHECK_EQ(HostBlobId(HostBlobId::first).slot(), 0u);
  CHECK(HostBlobId(HostBlobId::first + HostBlobId::count - 1u).valid());
  CHECK_EQ(HostBlobId(HostBlobId::first + HostBlobId::count - 1u).slot(), HostBlobId::count - 1u);
  CHECK_EQ(HostBlobId(HostBlobId::first + 77u).slot(), 77u); }

TEST(host_blob_id, an_id_just_outside_the_range_or_far_from_it_is_refused)
{ const std::uint32_t refused[] = {0u, 1u, HostBlobId::first - 1u, HostBlobId::first + HostBlobId::count,
                                   0xFFFFFFFFu};
  for (const std::uint32_t id : refused) CHECK(!HostBlobId(id).valid()); }
