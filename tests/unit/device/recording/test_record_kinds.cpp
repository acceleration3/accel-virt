#include "core/support/status.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/record_kinds.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::RecordKind;
using av::RecordKinds;
using av::Status;

namespace {

const RecordKind every[] = {RecordKind::context_setup, RecordKind::batch,      RecordKind::reply,
                            RecordKind::fence_completed, RecordKind::fault,    RecordKind::blob,
                            RecordKind::blob_bytes};

std::uint32_t kept(const RecordKinds &kinds)
{
    std::uint32_t count = 0u;
    for (const RecordKind kind : every) count += kinds.keeps(kind) ? 1u : 0u;
    return count;
}

}  // namespace

TEST(record_kinds, keeps_every_kind_but_a_blobs_bytes_by_default)
{ const RecordKinds kinds;
  CHECK(kinds.invariants_hold());
  CHECK_EQ(kept(kinds), 6u);
  CHECK(!kinds.keeps(RecordKind::blob_bytes));
  CHECK(kinds.keeps(RecordKind::blob));
  CHECK_EQ(kinds.mask(), RecordKinds::by_default); }

TEST(record_kinds_parse, names_one_kind_each)
{ struct Case { const char *list; RecordKind kind; };
  const Case cases[] = {{"setup", RecordKind::context_setup}, {"batch", RecordKind::batch},
                        {"reply", RecordKind::reply},         {"fence", RecordKind::fence_completed},
                        {"fault", RecordKind::fault},         {"blob", RecordKind::blob},
                        {"blob_bytes", RecordKind::blob_bytes}};
  for (const Case &c : cases) {
      RecordKinds kinds;
      CHECK_STATUS(kinds.parse(c.list), Status::ok);
      CHECK(kinds.invariants_hold());
      CHECK_EQ(kept(kinds), 1u);
      CHECK(kinds.keeps(c.kind));
  } }

TEST(record_kinds_parse, all_names_every_kind_and_a_list_names_what_it_lists)
{ RecordKinds kinds;
  CHECK_STATUS(kinds.parse("all"), Status::ok);
  CHECK_EQ(kept(kinds), 7u);
  CHECK(kinds.invariants_hold());
  CHECK_STATUS(kinds.parse("batch,reply,blob_bytes"), Status::ok);
  CHECK_EQ(kept(kinds), 3u);
  CHECK(kinds.keeps(RecordKind::batch));
  CHECK(kinds.keeps(RecordKind::reply));
  CHECK(kinds.keeps(RecordKind::blob_bytes));
  CHECK(!kinds.keeps(RecordKind::fault));
  CHECK(kinds.invariants_hold()); }

TEST(record_kinds_parse, a_list_that_names_nothing_is_the_default)
{ const char *const lists[] = {"", ",", ",,,", nullptr};
  for (const char *const list : lists) {
      RecordKinds kinds;
      CHECK_STATUS(kinds.parse("batch"), Status::ok);
      CHECK_STATUS(kinds.parse(list), Status::ok);
      CHECK_EQ(kinds.mask(), RecordKinds::by_default);
      CHECK(kinds.invariants_hold());
  } }

TEST(record_kinds_parse, refuses_a_name_no_kind_has_and_keeps_what_it_had)
{ const char *const lists[] = {"batches", "Batch", "", "blob_byte", "blob_bytess",
                               "batch,nonsense", "nonsense,batch", "a_very_long_name_indeed"};
  for (const char *const list : lists) {
      RecordKinds kinds;
      CHECK_STATUS(kinds.parse("fault"), Status::ok);
      const std::uint32_t before = kinds.mask();
      const Status status = kinds.parse(list);
      if (list[0] == '\0') {
          CHECK_STATUS(status, Status::ok);
          continue;
      }
      CHECK_STATUS(status, Status::invalid_parameter);
      CHECK_EQ(kinds.mask(), before);
      CHECK(kinds.invariants_hold());
  } }

TEST(record_kinds_parse, refuses_a_property_longer_than_any_list)
{ char list[512];
  for (std::size_t index = 0u; index + 1u < sizeof list; index++) list[index] = ',';
  list[sizeof list - 1u] = '\0';
  RecordKinds kinds;
  CHECK_STATUS(kinds.parse(list), Status::invalid_parameter);
  CHECK_EQ(kinds.mask(), RecordKinds::by_default);
  CHECK(kinds.invariants_hold()); }
