/* The ids host blobs take: every id from `first` up, and no id below it, so
 * the device refuses a transport blob's, a scanout's or the cursor's. */
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdint>

using av::HostBlobId;

TEST(host_blob_id, every_id_from_the_first_to_the_last_a_resource_names_is_valid)
{ CHECK(HostBlobId(HostBlobId::first).valid());
  CHECK(HostBlobId(HostBlobId::first + 16384u).valid());
  CHECK(HostBlobId(0xFFFFFFFFu).valid());
  CHECK_EQ(HostBlobId::most, 0xFF000000u);
  CHECK_EQ(HostBlobId::first + (HostBlobId::most - 1u), 0xFFFFFFFFu); }

TEST(host_blob_id, an_id_below_the_range_is_refused)
{ const std::uint32_t refused[] = {0u, 1u, 0x1000u, HostBlobId::first - 1u};
  for (const std::uint32_t id : refused) CHECK(!HostBlobId(id).valid()); }
