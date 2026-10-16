/* Which Share an id names, in which domain: a table that grows as the guest
 * shares more, so an id past any count still opens, and a lookup that stays
 * one probe sequence however many ids came and went. */
#include "executor/sharing/share.hpp"
#include "executor/sharing/share_index.hpp"
#include "harness/executor/share_index_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Share;
using av::ShareIndex;
using av::ShareIndexProbe;
using av::ShareOrigin;
using av::Status;

namespace {

Share first(1u, 1u, 0u, 0u, ShareOrigin::object);
Share second(1u, 2u, 0u, 0u, ShareOrigin::object);

}  // namespace

TEST(share_index, names_a_share_by_domain_and_id)
{ MockPlatform platform;
  ShareIndex index(platform);
  CHECK(index.find(1u, 7u) == nullptr);
  CHECK_STATUS(index.insert(1u, 7u, &first), Status::ok);
  CHECK_STATUS(index.insert(2u, 7u, &second), Status::ok);
  CHECK(index.find(1u, 7u) == &first);
  CHECK(index.find(2u, 7u) == &second);   /* another guest device's id 7 */
  CHECK(index.find(3u, 7u) == nullptr);
  CHECK(index.find(1u, 8u) == nullptr);
  CHECK_EQ(index.count(), 2u); }

/* Every guest device mints from 1, so one id is in every domain at once: each
 * names its own share, however their probes cross. The index never reads a
 * share, so distinct addresses stand for distinct shares. */
TEST(share_index, one_id_in_many_domains_names_each_domain_s_own)
{ MockPlatform platform;
  ShareIndex index(platform);
  static unsigned char marks[48];
  const auto share_of = [](std::uint32_t domain) { return reinterpret_cast<Share *>(&marks[domain - 1u]); };
  for (std::uint32_t domain = 1u; domain <= 48u; domain++)
      CHECK_STATUS(index.insert(domain, 7u, share_of(domain)), Status::ok);
  for (std::uint32_t domain = 1u; domain <= 48u; domain++) CHECK(index.find(domain, 7u) == share_of(domain));
  CHECK(index.find(49u, 7u) == nullptr);
  for (std::uint32_t domain = 1u; domain <= 48u; domain += 2u) index.remove(domain, 7u);
  for (std::uint32_t domain = 2u; domain <= 48u; domain += 2u) CHECK(index.find(domain, 7u) == share_of(domain));
  for (std::uint32_t domain = 1u; domain <= 48u; domain += 2u) CHECK(index.find(domain, 7u) == nullptr);
  CHECK_EQ(index.count(), 24u); }

/* A pair already naming a share is refused rather than replaced: the guest
 * that shared it still holds it. */
TEST(share_index, a_pair_already_named_is_a_conflict)
{ MockPlatform platform;
  ShareIndex index(platform);
  CHECK_STATUS(index.insert(1u, 7u, &first), Status::ok);
  CHECK_STATUS(index.insert(1u, 7u, &second), Status::conflict);
  CHECK(index.find(1u, 7u) == &first);
  CHECK_EQ(index.count(), 1u); }

/* The miniport never mints 0, and a share is something. */
TEST(share_index, id_0_and_no_share_are_refused)
{ MockPlatform platform;
  ShareIndex index(platform);
  CHECK_STATUS(index.insert(1u, 0u, &first), Status::invalid_parameter);
  CHECK_STATUS(index.insert(1u, 7u, nullptr), Status::invalid_parameter);
  CHECK(index.find(1u, 0u) == nullptr);
  CHECK_EQ(index.count(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

/* The fixed tables this replaces stopped opening at 256 ids, and DWM holds
 * more Windows.Graphics.Capture buffers than that. */
TEST(share_index, grows_past_any_fixed_count)
{ MockPlatform platform;
  ShareIndex index(platform);
  constexpr std::uint64_t many = 5000u;
  for (std::uint64_t id = 1u; id <= many; id++) CHECK_STATUS(index.insert(1u, id, &first), Status::ok);
  CHECK_EQ(index.count(), static_cast<std::uint32_t>(many));
  for (std::uint64_t id = 1u; id <= many; id++) CHECK(index.find(1u, id) == &first);
  CHECK(index.find(1u, many + 1u) == nullptr);
  for (std::uint64_t id = 1u; id <= many; id++) index.remove(1u, id);
  CHECK_EQ(index.count(), 0u);
  CHECK_EQ(ShareIndexProbe(index).domains(), 0u); }

TEST(share_index, a_removed_pair_names_nothing_and_its_neighbours_still_open)
{ MockPlatform platform;
  ShareIndex index(platform);
  for (std::uint64_t id = 1u; id <= 20u; id++) CHECK_STATUS(index.insert(1u, id, &first), Status::ok);
  index.remove(1u, 10u);
  CHECK(index.find(1u, 10u) == nullptr);
  for (std::uint64_t id = 1u; id <= 20u; id++)
      if (id != 10u) CHECK(index.find(1u, id) == &first);
  CHECK_EQ(index.count(), 19u);
  CHECK_STATUS(index.insert(1u, 10u, &second), Status::ok);   /* reminted after a release */
  CHECK(index.find(1u, 10u) == &second);
  CHECK_EQ(index.count(), 20u); }

TEST(share_index, removing_what_it_does_not_name_is_nothing)
{ MockPlatform platform;
  ShareIndex index(platform);
  index.remove(1u, 7u);                  /* an empty table */
  CHECK_STATUS(index.insert(1u, 7u, &first), Status::ok);
  index.remove(1u, 0u);
  index.remove(1u, 8u);
  index.remove(2u, 7u);
  index.remove(1u, 7u);
  index.remove(1u, 7u);                  /* twice */
  CHECK_EQ(index.count(), 0u);
  CHECK(index.find(1u, 7u) == nullptr); }

/* Ids come and go for as long as the guest runs - a window presenting mints
 * and releases one per frame - so tombstones must not fill the table. */
TEST(share_index, churn_does_not_fill_the_table)
{ MockPlatform platform;
  ShareIndex index(platform);
  for (std::uint64_t id = 1u; id <= 100000u; id++) {
      CHECK_STATUS(index.insert(1u, id, &first), Status::ok);
      if (id > 3u) index.remove(1u, id - 3u);
  }
  CHECK_EQ(index.count(), 3u);
  CHECK_EQ(ShareIndexProbe(index).buckets(1u), (av::KeyedSlots<std::uint64_t, Share *>::first_buckets));
  CHECK(index.find(1u, 100000u) == &first);
  CHECK(index.find(1u, 99997u) == nullptr); }

/* A domain's table is made with its first id and given back with its last:
 * a guest device that shares nothing holds nothing. */
TEST(share_index, a_domain_s_table_lives_from_its_first_id_to_its_last)
{ MockPlatform platform;
  ShareIndex index(platform);
  CHECK_EQ(ShareIndexProbe(index).domains(), 0u);
  CHECK_STATUS(index.insert(3u, 7u, &first), Status::ok);
  CHECK_STATUS(index.insert(3u, 8u, &second), Status::ok);
  CHECK_EQ(ShareIndexProbe(index).domains(), 1u);
  CHECK(ShareIndexProbe(index).buckets(3u) != 0u);
  index.remove(3u, 7u);
  CHECK_EQ(ShareIndexProbe(index).domains(), 1u);
  index.remove(3u, 8u);
  CHECK_EQ(ShareIndexProbe(index).domains(), 0u);
  CHECK_EQ(ShareIndexProbe(index).buckets(3u), 0u);
  CHECK(index.find(3u, 8u) == nullptr); }

/* Each place memory is asked for refusing in turn - the domain list, the
 * domain's table, its ids - adds nothing and leaves no table behind. */
TEST(share_index, a_table_that_cannot_grow_adds_nothing)
{ for (std::uint32_t failing = 1u; failing <= 5u; failing++) {
      MockPlatform platform;
      ShareIndex index(platform);
      platform.fail_allocation_at(failing);
      CHECK_STATUS(index.insert(1u, 7u, &first), Status::no_memory);
      CHECK(index.find(1u, 7u) == nullptr);
      CHECK_EQ(index.count(), 0u);
      CHECK_EQ(ShareIndexProbe(index).domains(), 0u);
  }
  MockPlatform platform;
  ShareIndex index(platform);
  for (std::uint64_t id = 1u; id <= 8u; id++) CHECK_STATUS(index.insert(1u, id, &first), Status::ok);
  platform.fail_allocation_at(1u);        /* the 9th needs a bigger table */
  CHECK_STATUS(index.insert(1u, 9u, &second), Status::no_memory);
  CHECK(index.find(1u, 9u) == nullptr);
  for (std::uint64_t id = 1u; id <= 8u; id++) CHECK(index.find(1u, id) == &first);
  CHECK_EQ(index.count(), 8u); }
