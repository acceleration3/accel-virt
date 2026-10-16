/* The cl_mems standing in for GL objects (ClGlMirrors): each found by its
 * cl_mem until forgotten, which hands back what it was, however many are held
 * - as many as the guest holds cl_mems; everything the table refuses; its
 * memory given back at the end; and the invariant broken each way it can be. */
#include "executor/opencl/cl_gl_mirrors.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

struct ClGlMirrorsProbe {
    static GlMirror &held(ClGlMirrors &mirrors, const void *mem)
    {
        return *mirrors.mirrors_.find(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mem)));
    }
};

}  // namespace av

using av::ClGlMirrors;
using av::ClGlMirrorsProbe;
using av::GlMirror;
using av::MockPlatform;
using av::Status;

namespace {

const void *mem_at(std::uintptr_t at) { return reinterpret_cast<const void *>(at); }
GlMirror    buffer_at(std::uintptr_t at, std::uint32_t name) { return GlMirror(mem_at(at), name, 64u, true); }

/* The `k`th of up to 2^24 distinct cl_mems, scattered as a heap's are: an
 * odd multiplier is a bijection modulo 2^24, and the step is an allocation's
 * alignment. Neighbours collide in the index as often as real pointers do. */
const void *scattered(std::uint32_t k)
{
    constexpr std::uintptr_t heap = 0x5A5A00000000u;
    return mem_at(heap + ((std::uintptr_t{k} * 2654435761u) & 0xFFFFFFu) * 16u);
}
GlMirror    mirror_of(std::uint32_t k) { return GlMirror(scattered(k), k + 1u, 64u, true); }

/* Every one of `first` to `last` is held, as itself. */
bool all_found(ClGlMirrors &mirrors, std::uint32_t first, std::uint32_t last)
{
    for (std::uint32_t k = first; k < last; k++) {
        const GlMirror *const found = mirrors.find(scattered(k));
        if (found == nullptr || found->name() != k + 1u) return false;
    }
    return true;
}

}  // namespace

TEST(cl_gl_mirrors, a_remembered_mirror_is_found_by_its_mem_until_it_is_forgotten)
{ MockPlatform platform;
  ClGlMirrors  mirrors(platform);
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.find(mem_at(0x1000u)) == nullptr);
  CHECK(mirrors.remember(buffer_at(0x1000u, 7u)) == Status::ok);
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.remember(buffer_at(0x2000u, 8u)) == Status::ok);
  CHECK(mirrors.invariants_hold());
  CHECK_EQ(mirrors.count(), 2u);
  GlMirror *const found = mirrors.find(mem_at(0x2000u));
  CHECK(found != nullptr);
  CHECK_EQ(found->name(), 8u);
  /* What is found is the held one: its turn is the table's from then on. */
  found->acquire();
  CHECK(mirrors.find(mem_at(0x2000u))->acquired());
  CHECK(mirrors.find(nullptr) == nullptr);
  CHECK(mirrors.find(mem_at(0x3000u)) == nullptr);

  GlMirror gone;
  CHECK(mirrors.forget(mem_at(0x1000u), gone));
  CHECK(mirrors.invariants_hold());
  CHECK_EQ(gone.name(), 7u);
  CHECK(mirrors.find(mem_at(0x1000u)) == nullptr);
  /* The other is where it was, and still itself. */
  CHECK_EQ(mirrors.find(mem_at(0x2000u))->name(), 8u);
  CHECK(mirrors.find(mem_at(0x2000u))->acquired());
  /* A mem not held: nothing forgotten, `gone` as it was. */
  CHECK(!mirrors.forget(mem_at(0x1000u), gone));
  CHECK_EQ(gone.name(), 7u);
  CHECK_EQ(mirrors.count(), 1u);
  /* The last held, forgotten where it is. */
  CHECK(mirrors.forget(mem_at(0x2000u), gone));
  CHECK(mirrors.invariants_hold());
  CHECK_EQ(gone.name(), 8u);
  CHECK_EQ(mirrors.count(), 0u);
  CHECK(mirrors.find(mem_at(0x2000u)) == nullptr); }

TEST(cl_gl_mirrors, a_mirror_of_nothing_a_mem_held_twice_and_a_table_that_cannot_grow_are_refused)
{ MockPlatform platform;
  ClGlMirrors  mirrors(platform);
  CHECK(mirrors.remember(GlMirror()) == Status::invalid_parameter);
  CHECK(mirrors.remember(GlMirror(nullptr, 7u, 64u, true)) == Status::invalid_parameter);
  CHECK(mirrors.remember(mirror_of(0u)) == Status::ok);
  CHECK(mirrors.remember(GlMirror(scattered(0u), 99u, 64u, true)) == Status::conflict);
  CHECK_EQ(mirrors.find(scattered(0u))->name(), 1u);
  std::uint32_t k = 1u;
  platform.fail_allocation_at(1u);
  while (mirrors.remember(mirror_of(k)) == Status::ok) k++;
  CHECK(mirrors.find(scattered(k)) == nullptr);
  CHECK(all_found(mirrors, 0u, k));
  CHECK_EQ(mirrors.count(), k);
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.remember(mirror_of(k)) == Status::ok);
  CHECK(all_found(mirrors, 0u, k + 1u)); }

/* A texture pool, as DaVinci Resolve keeps one for its whole session: well past
 * the 2048 the table was once sized to; a seventh let go from the middle in no
 * order and new ones made in their place, every one still held found as
 * itself; then all of them forgotten one at a time, each as itself. */
TEST(cl_gl_mirrors, five_thousand_held_at_once_are_each_found_through_churn_and_forgotten)
{ MockPlatform platform;
  ClGlMirrors  mirrors(platform);
  constexpr std::uint32_t pool = 5005u;
  static_assert(pool % 7u == 0u);
  for (std::uint32_t k = 0u; k < pool; k++) CHECK(mirrors.remember(mirror_of(k)) == Status::ok);
  CHECK(mirrors.invariants_hold());
  CHECK(all_found(mirrors, 0u, pool));
  std::uint32_t made = pool;
  for (std::uint32_t step = 0u; step < pool / 7u; step++) {
      const std::uint32_t k = (step * 37u) % (pool / 7u) * 7u;
      GlMirror            gone;
      CHECK(mirrors.forget(scattered(k), gone));
      CHECK_EQ(gone.name(), k + 1u);
      CHECK(mirrors.find(scattered(k)) == nullptr);
      CHECK(mirrors.remember(mirror_of(made++)) == Status::ok);
  }
  CHECK(mirrors.invariants_hold());
  CHECK_EQ(mirrors.count(), pool);
  CHECK(all_found(mirrors, pool, made));
  bool kept = true;
  for (std::uint32_t k = 0u; k < pool; k++) kept = kept && (mirrors.find(scattered(k)) != nullptr) == (k % 7u != 0u);
  CHECK(kept);
  GlMirror      gone;
  std::uint32_t forgotten = 0u;
  while (mirrors.forget_any(gone)) {
      forgotten++;
      CHECK(gone.described());
      CHECK(mirrors.find(gone.mem()) == nullptr);
  }
  CHECK_EQ(forgotten, pool);
  CHECK_EQ(mirrors.count(), 0u);
  CHECK(mirrors.invariants_hold()); }

/* Nothing is held until a mirror is; forget_all gives every byte back, and the
 * table fills again after. */
TEST(cl_gl_mirrors, forgetting_everything_gives_the_table_back)
{ MockPlatform platform;
  ClGlMirrors  mirrors(platform);
  GlMirror     gone;
  CHECK(!mirrors.forget(scattered(0u), gone));
  CHECK(!mirrors.forget_any(gone));
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(mirrors.remember(mirror_of(1u)) == Status::ok);
  CHECK(mirrors.remember(mirror_of(2u)) == Status::ok);
  mirrors.forget_all();
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_EQ(mirrors.count(), 0u);
  CHECK(mirrors.find(scattered(1u)) == nullptr);
  CHECK(mirrors.remember(mirror_of(1u)) == Status::ok);
  CHECK_EQ(mirrors.find(scattered(1u))->name(), 2u);
  mirrors.forget_all();
  mirrors.forget_all();
  CHECK(mirrors.invariants_hold()); }

TEST(cl_gl_mirrors, the_invariant_fails_each_way_the_table_can_be_wrong)
{ MockPlatform platform;
  /* A mirror of nothing held. */
  { ClGlMirrors mirrors(platform);
    CHECK(mirrors.remember(mirror_of(0u)) == Status::ok);
    ClGlMirrorsProbe::held(mirrors, scattered(0u)) = GlMirror();
    CHECK(!mirrors.invariants_hold()); }
  /* A mirror held under another mem than its own. */
  { ClGlMirrors mirrors(platform);
    CHECK(mirrors.remember(mirror_of(0u)) == Status::ok);
    ClGlMirrorsProbe::held(mirrors, scattered(0u)) = mirror_of(1u);
    CHECK(!mirrors.invariants_hold()); } }
