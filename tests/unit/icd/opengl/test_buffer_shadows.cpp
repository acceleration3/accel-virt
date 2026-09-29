/* What the host holds of one context's buffers, as the guest knows it: a copy
 * kept and replaced, a range inside it answered and one past it not, the
 * least recently used copy dropped for a slot or for the budget, glBufferSubData
 * written into a copy, fresh storage marked until anything writes it, every
 * way a copy is forgotten, and a distrusted context letting go of what it
 * holds on its own thread. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/buffer_shadow.hpp"
#include "icd/opengl/buffer_shadows.hpp"

#include <cstdint>
#include <cstring>

using av::MockPlatform;
using av::glicd::BufferShadow;
using av::glicd::BufferShadows;

namespace av::glicd {

struct BufferShadowsProbe {
    static void miscount(BufferShadows &shadows) { shadows.held_bytes_++; }
    static void set_held_bytes(BufferShadows &shadows, std::uint64_t bytes) { shadows.held_bytes_ = bytes; }
    static void set_next_fresh(BufferShadows &shadows, std::uint32_t next) { shadows.next_fresh_ = next; }
    static BufferShadow &shadow(BufferShadows &shadows, std::uint32_t slot) { return shadows.shadows_[slot].object(); }
    /* A slot constructed and holding nothing. */
    static void empty_slot(BufferShadows &shadows, BlockSource &memory)
    {
        static_cast<void>(shadows.shadows_[5].construct(memory));
    }
    /* A second slot claiming the first slot's buffer. */
    static void duplicate(BufferShadows &shadows, BlockSource &memory, const std::uint8_t *bytes)
    {
        static_cast<void>(shadows.shadows_[1].construct(memory));
        static_cast<void>(shadows.shadows_[1].object().hold(shadows.shadows_[0].object().buffer(), 0u, 1u, bytes, 1u));
    }
};

struct BufferShadowProbe {
    static void set_length(BufferShadow &shadow, std::uint64_t length) { shadow.length_ = length; }
    static void set_buffer(BufferShadow &shadow, std::uint32_t buffer) { shadow.buffer_ = buffer; }
};

}  // namespace av::glicd

namespace {

using Probe = av::glicd::BufferShadowsProbe;

void pattern(std::uint8_t *bytes, std::size_t size, std::uint8_t seed)
{
    for (std::size_t at = 0u; at < size; at++) bytes[at] = static_cast<std::uint8_t>(at * 3u + seed);
}

/* Big enough for two copies to pass the budget together and not alone. */
std::uint8_t large[40u << 20u];

}  // namespace

TEST(buffer_shadows, a_copy_answers_ranges_inside_it_and_is_replaced_by_one_that_is_not)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  CHECK(shadows.trusted());
  std::uint8_t bytes[64];
  pattern(bytes, sizeof bytes, 1u);
  shadows.keep(7u, 0u, sizeof bytes, bytes);
  CHECK_EQ(shadows.held(), 1u);
  CHECK(shadows.invariants_hold());
  const std::uint8_t *const inside = shadows.covering(7u, 8u, 16u);
  CHECK(inside != nullptr);
  CHECK(std::memcmp(inside, bytes + 8u, 16u) == 0);
  CHECK(shadows.covering(7u, 60u, 8u) == nullptr);
  CHECK(shadows.covering(8u, 0u, 1u) == nullptr);
  CHECK(shadows.covering(0u, 0u, 1u) == nullptr);
  /* Inside the copy: copied in. */
  std::uint8_t other[8];
  pattern(other, sizeof other, 90u);
  shadows.keep(7u, 16u, sizeof other, other);
  CHECK_EQ(shadows.held(), 1u);
  CHECK(std::memcmp(shadows.covering(7u, 16u, 8u), other, sizeof other) == 0);
  CHECK(std::memcmp(shadows.covering(7u, 0u, 16u), bytes, 16u) == 0);
  /* Reaching past it: the copy is of the new range alone. */
  shadows.keep(7u, 32u, sizeof bytes, bytes);
  CHECK_EQ(shadows.held(), 1u);
  CHECK(shadows.covering(7u, 0u, 1u) == nullptr);
  CHECK(std::memcmp(shadows.covering(7u, 32u, 64u), bytes, sizeof bytes) == 0);
  CHECK(shadows.invariants_hold()); }

TEST(buffer_shadows, keeps_nothing_of_no_buffer_no_bytes_a_range_past_the_budget_or_no_memory)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  std::uint8_t  bytes[8] = {};
  shadows.keep(0u, 0u, sizeof bytes, bytes);
  shadows.keep(7u, 0u, 0u, bytes);
  /* Refused on its length alone: nothing is read. */
  shadows.keep(7u, 0u, BufferShadows::max_bytes + 1u, bytes);
  CHECK_EQ(shadows.held(), 0u);
  platform.fail_allocation_at(1u);
  shadows.keep(7u, 0u, sizeof bytes, bytes);
  CHECK_EQ(shadows.held(), 0u);
  CHECK(shadows.covering(7u, 0u, 1u) == nullptr);
  CHECK(shadows.invariants_hold()); }

TEST(buffer_shadows, the_least_recently_used_copy_goes_for_a_slot)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  std::uint8_t  bytes[8] = {};
  for (std::uint32_t buffer = 1u; buffer <= BufferShadows::max_shadows; buffer++)
    shadows.keep(buffer, 0u, sizeof bytes, bytes);
  CHECK_EQ(shadows.held(), BufferShadows::max_shadows);
  /* Buffer 1 is used again, so buffer 2 is the least recent. */
  CHECK(shadows.covering(1u, 0u, 1u) != nullptr);
  shadows.keep(BufferShadows::max_shadows + 1u, 0u, sizeof bytes, bytes);
  CHECK_EQ(shadows.held(), BufferShadows::max_shadows);
  CHECK(shadows.covering(2u, 0u, 1u) == nullptr);
  CHECK(shadows.covering(1u, 0u, 1u) != nullptr);
  CHECK(shadows.covering(3u, 0u, 1u) != nullptr);
  CHECK(shadows.covering(BufferShadows::max_shadows + 1u, 0u, 1u) != nullptr);
  CHECK(shadows.invariants_hold()); }

TEST(buffer_shadows, the_least_recently_used_copies_go_for_the_budget)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  const std::uint64_t mib = std::uint64_t{1} << 20u;
  std::uint8_t        bytes[8] = {};
  shadows.keep(1u, 0u, sizeof bytes, bytes);
  shadows.keep(2u, 0u, 30u * mib, large);
  shadows.keep(3u, 0u, sizeof bytes, bytes);
  /* 30 MiB and 40 MiB pass it together: the oldest go until they fit - the
   * small first copy, then the 30 MiB one - and the newest small one stays. */
  shadows.keep(4u, 0u, 40u * mib, large);
  CHECK_EQ(shadows.held(), 2u);
  CHECK(shadows.covering(1u, 0u, 1u) == nullptr);
  CHECK(shadows.covering(2u, 0u, 1u) == nullptr);
  CHECK(shadows.covering(3u, 0u, 1u) != nullptr);
  CHECK(shadows.covering(4u, 0u, 40u * mib) != nullptr);
  CHECK(shadows.invariants_hold()); }

TEST(buffer_shadows, buffer_sub_data_goes_into_a_copy_and_naming_no_bytes_forgets_it)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  std::uint8_t  zeros[16] = {};
  shadows.keep(7u, 0u, sizeof zeros, zeros);
  std::uint8_t source[4];
  pattern(source, sizeof source, 60u);
  shadows.wrote(7u, 4u, sizeof source, source);
  const std::uint8_t *const copy = shadows.covering(7u, 0u, 16u);
  CHECK(std::memcmp(copy + 4u, source, sizeof source) == 0);
  CHECK_EQ(copy[3], 0u);
  CHECK_EQ(copy[8], 0u);
  /* No copy of buffer 9: nothing to write into. */
  shadows.wrote(9u, 0u, sizeof source, source);
  CHECK_EQ(shadows.held(), 1u);
  shadows.wrote(7u, 0u, 4u, nullptr);
  CHECK_EQ(shadows.held(), 0u);
  /* Written storage is no longer fresh. */
  shadows.respecify(8u, false);
  shadows.wrote(8u, 0u, sizeof source, source);
  CHECK(!shadows.fresh(8u));
  CHECK(shadows.invariants_hold()); }

TEST(buffer_shadows, fresh_storage_is_marked_until_anything_writes_it)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  std::uint8_t  bytes[4] = {};
  CHECK(!shadows.fresh(0u));
  shadows.respecify(7u, false);
  CHECK(shadows.fresh(7u));
  CHECK(!shadows.fresh(0u));
  CHECK(!shadows.fresh(8u));
  /* Storage made with data is not fresh, and neither is no buffer. */
  shadows.respecify(7u, true);
  CHECK(!shadows.fresh(7u));
  shadows.respecify(0u, false);
  CHECK(!shadows.fresh(0u));
  /* A copy kept of it, and a forget, end the mark. */
  shadows.respecify(7u, false);
  shadows.keep(7u, 0u, sizeof bytes, bytes);
  CHECK(!shadows.fresh(7u));
  shadows.respecify(8u, false);
  shadows.forget(8u);
  CHECK(!shadows.fresh(8u));
  /* New storage drops the old copy. */
  shadows.respecify(7u, false);
  CHECK(shadows.covering(7u, 0u, 1u) == nullptr);
  CHECK(shadows.fresh(7u));
  /* Past max_fresh marks the oldest is lost. */
  for (std::uint32_t buffer = 100u; buffer < 100u + BufferShadows::max_fresh; buffer++)
    shadows.respecify(buffer, false);
  CHECK(!shadows.fresh(7u));
  CHECK(shadows.fresh(100u));
  CHECK(shadows.fresh(100u + BufferShadows::max_fresh - 1u));
  CHECK(shadows.invariants_hold()); }

TEST(buffer_shadows, forgets_by_name_by_list_and_all)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  std::uint8_t  bytes[4] = {};
  for (std::uint32_t buffer = 1u; buffer <= 3u; buffer++) shadows.keep(buffer, 0u, sizeof bytes, bytes);
  shadows.forget(2u);
  shadows.forget(9u);
  CHECK_EQ(shadows.held(), 2u);
  const std::uint32_t deleted[] = {0u, 9u, 1u};
  shadows.forget(1, nullptr);
  shadows.forget(0, deleted);
  CHECK_EQ(shadows.held(), 2u);
  shadows.forget(3, deleted);
  CHECK_EQ(shadows.held(), 1u);
  CHECK(shadows.covering(3u, 0u, 1u) != nullptr);
  shadows.respecify(5u, false);
  shadows.forget_all();
  CHECK_EQ(shadows.held(), 0u);
  CHECK(!shadows.fresh(5u));
  CHECK(shadows.trusted());
  CHECK(shadows.invariants_hold()); }

/* Another thread only sets the flag; what is held goes at the next ask. */
TEST(buffer_shadows, a_distrusted_context_keeps_nothing_and_lets_go_on_its_own_thread)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  std::uint8_t  bytes[4] = {};
  shadows.keep(1u, 0u, sizeof bytes, bytes);
  shadows.respecify(2u, false);
  shadows.distrust();
  CHECK(!shadows.trusted());
  CHECK_EQ(shadows.held(), 1u);
  CHECK(shadows.invariants_hold());
  CHECK(!shadows.fresh(2u));
  CHECK(shadows.covering(1u, 0u, 1u) == nullptr);
  CHECK_EQ(shadows.held(), 0u);
  shadows.keep(1u, 0u, sizeof bytes, bytes);
  shadows.respecify(3u, false);
  CHECK_EQ(shadows.held(), 0u);
  CHECK(!shadows.fresh(3u));
  /* Forgetting everything does not trust again. */
  shadows.forget_all();
  CHECK(!shadows.trusted());
  CHECK(shadows.invariants_hold()); }

TEST(buffer_shadows, reports_trust_and_each_copy)
{ MockPlatform  platform;
  BufferShadows shadows{platform};
  std::uint8_t  bytes[32] = {};
  shadows.keep(7u, 12u, sizeof bytes, bytes);
  char            text[256];
  av::StateWriter writer(text, sizeof text);
  shadows.report(writer);
  CHECK(std::strcmp(writer.dump(), "shadows/0 trusted=true held=1 held_bytes=32\n"
                                   "shadows/0/shadow/0 buffer=7 offset=12 length=32 stamp=1") == 0); }

TEST(buffer_shadows_invariants, fail_for_a_miscount_a_copy_twice_a_broken_or_empty_copy_or_a_bad_ring)
{ MockPlatform platform;
  std::uint8_t bytes[4] = {};
  BufferShadows counted{platform};
  counted.keep(1u, 0u, sizeof bytes, bytes);
  Probe::miscount(counted);
  CHECK(!counted.invariants_hold());
  BufferShadows twice{platform};
  twice.keep(1u, 0u, sizeof bytes, bytes);
  Probe::duplicate(twice, platform, bytes);
  CHECK(!twice.invariants_hold());
  BufferShadows broken{platform};
  broken.keep(1u, 0u, sizeof bytes, bytes);
  av::glicd::BufferShadowProbe::set_buffer(Probe::shadow(broken, 0u), 0u);
  CHECK(!broken.invariants_hold());
  av::glicd::BufferShadowProbe::set_buffer(Probe::shadow(broken, 0u), 1u);
  CHECK(broken.invariants_hold());
  BufferShadows empty{platform};
  Probe::empty_slot(empty, platform);
  CHECK(!empty.invariants_hold());
  /* Counted right, but past the budget. */
  BufferShadows over{platform};
  over.keep(1u, 0u, sizeof bytes, bytes);
  av::glicd::BufferShadowProbe::set_length(Probe::shadow(over, 0u), BufferShadows::max_bytes + 1u);
  Probe::set_held_bytes(over, BufferShadows::max_bytes + 1u);
  CHECK(!over.invariants_hold());
  av::glicd::BufferShadowProbe::set_length(Probe::shadow(over, 0u), sizeof bytes);
  Probe::set_held_bytes(over, sizeof bytes);
  CHECK(over.invariants_hold());
  BufferShadows ring{platform};
  Probe::set_next_fresh(ring, BufferShadows::max_fresh);
  CHECK(!ring.invariants_hold()); }
