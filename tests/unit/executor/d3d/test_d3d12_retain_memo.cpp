#include "executor/d3d/d3d12_block_heap.hpp"
#include "executor/d3d/d3d12_retain_memo.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::CountedObject;
using av::D3d12BlockHeap;
using av::D3d12RetainMemo;

TEST(d3d12_retain_memo, an_object_noted_for_a_list_covers_that_pair_and_no_other)
{ D3d12RetainMemo memo;
  CountedObject   first, second;
  CHECK(!memo.covers(3u, &first, nullptr));
  memo.note(3u, &first, false);
  CHECK(memo.covers(3u, &first, nullptr));
  CHECK(!memo.covers(4u, &first, nullptr));
  CHECK(!memo.covers(3u, &second, nullptr)); }

TEST(d3d12_retain_memo, a_pin_is_covered_only_once_the_hold_has_one)
{ D3d12RetainMemo memo;
  CountedObject   object;
  auto *const     pin = reinterpret_cast<D3d12BlockHeap *>(std::uintptr_t{0x1000u});
  memo.note(3u, &object, false);
  CHECK(memo.covers(3u, &object, nullptr));
  CHECK(!memo.covers(3u, &object, pin));
  memo.note(3u, &object, true);
  CHECK(memo.covers(3u, &object, pin));
  memo.note(3u, &object, false);
  CHECK(memo.covers(3u, &object, pin)); }

TEST(d3d12_retain_memo, whole_is_only_what_was_noted_whole)
{ D3d12RetainMemo memo;
  CountedObject   object;
  memo.note(3u, &object, true);
  CHECK(!memo.whole(3u, &object));
  memo.note_whole(3u, &object);
  CHECK(memo.whole(3u, &object));
  CHECK(!memo.whole(4u, &object));
  memo.note(3u, &object, true);
  CHECK(memo.whole(3u, &object)); }

TEST(d3d12_retain_memo, clearing_forgets_everything_and_a_colliding_object_replaces_the_older_one)
{ D3d12RetainMemo memo;
  CountedObject   objects[64];
  for (CountedObject &object : objects) memo.note(1u, &object, false);
  std::uint32_t kept = 0u;
  for (CountedObject &object : objects)
      if (memo.covers(1u, &object, nullptr)) kept++;
  CHECK(kept > 0u);
  CHECK(kept <= 8u);
  memo.clear();
  for (CountedObject &object : objects) CHECK(!memo.covers(1u, &object, nullptr)); }
