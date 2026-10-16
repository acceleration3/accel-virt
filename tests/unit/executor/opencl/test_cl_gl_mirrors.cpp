/* The cl_mems standing in for GL objects (ClGlMirrors): each found by its
 * cl_mem until forgotten, which hands back what it was, however many are held
 * - as many as the guest can hold cl_mems; everything the table refuses; its
 * memory, mapped at the first mirror and given back at the end; and the
 * invariant broken each way it can be. */
#include "executor/opencl/cl_gl_mirrors.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <sys/resource.h>

namespace av {

struct ClGlMirrorsProbe {
    static constexpr std::uint32_t buckets = ClGlMirrors::buckets;

    static bool mapped(const ClGlMirrors &mirrors) { return mirrors.table_ != nullptr; }
    static void count(ClGlMirrors &mirrors, std::uint32_t count) { mirrors.count_ = count; }
    static void put(ClGlMirrors &mirrors, std::uint32_t slot, const GlMirror &mirror) { mirrors.mirrors_[slot] = mirror; }
    static std::uint32_t entry(const ClGlMirrors &mirrors, std::uint32_t bucket) { return mirrors.index_[bucket]; }
    static void index(ClGlMirrors &mirrors, std::uint32_t bucket, std::uint32_t entry) { mirrors.index_[bucket] = entry; }
    /* The first bucket holding nothing. */
    static std::uint32_t empty_bucket(const ClGlMirrors &mirrors)
    {
        std::uint32_t bucket = 0u;
        while (mirrors.index_[bucket] != 0u) bucket++;
        return bucket;
    }
    /* The bucket holding `mem`'s slot. */
    static std::uint32_t bucket_of(const ClGlMirrors &mirrors, const void *mem)
    {
        std::uint32_t bucket = 0u;
        while (mirrors.index_[bucket] == 0u || mirrors.mirrors_[mirrors.index_[bucket] - 1u].mem() != mem) bucket++;
        return bucket;
    }
};

}  // namespace av

using av::ClGlMirrors;
using av::ClGlMirrorsProbe;
using av::GlMirror;

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
{ ClGlMirrors mirrors;
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.find(mem_at(0x1000u)) == nullptr);
  CHECK(mirrors.remember(buffer_at(0x1000u, 7u)));
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.remember(buffer_at(0x2000u, 8u)));
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
  /* The last moved into the hole and is still itself. */
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

TEST(cl_gl_mirrors, a_mirror_of_nothing_a_mem_held_twice_and_a_full_table_are_refused)
{ ClGlMirrors mirrors;
  CHECK(!mirrors.remember(GlMirror()));
  CHECK(!mirrors.remember(GlMirror(nullptr, 7u, 64u, true)));
  CHECK(mirrors.remember(mirror_of(0u)));
  CHECK(!mirrors.remember(GlMirror(scattered(0u), 99u, 64u, true)));
  CHECK_EQ(mirrors.find(scattered(0u))->name(), 1u);
  CHECK_EQ(mirrors.count(), 1u);
  for (std::uint32_t k = 1u; k < ClGlMirrors::max_mirrors; k++) CHECK(mirrors.remember(mirror_of(k)));
  CHECK(mirrors.invariants_hold());
  CHECK_EQ(mirrors.count(), ClGlMirrors::max_mirrors);
  CHECK(all_found(mirrors, 0u, ClGlMirrors::max_mirrors));
  CHECK(!mirrors.remember(mirror_of(ClGlMirrors::max_mirrors)));
  CHECK(mirrors.find(scattered(ClGlMirrors::max_mirrors)) == nullptr);
  CHECK(mirrors.invariants_hold());
  /* Forgotten one at a time to the last, then nothing. */
  GlMirror      gone;
  std::uint32_t forgotten = 0u;
  while (mirrors.forget_any(gone)) {
      forgotten++;
      CHECK(gone.described());
      CHECK(mirrors.find(gone.mem()) == nullptr);
      CHECK(mirrors.invariants_hold());
  }
  CHECK_EQ(forgotten, ClGlMirrors::max_mirrors);
  CHECK_EQ(mirrors.count(), 0u);
  CHECK(!mirrors.forget_any(gone)); }

/* A texture pool, as DaVinci Resolve keeps one for its whole session: far
 * more than 256 held at once, where the table used to refuse; a few let go
 * from the middle in no order and new ones made in their place, every one
 * still held found as itself after each; then all of them forgotten one by
 * one, each as itself. */
TEST(cl_gl_mirrors, more_than_256_held_at_once_are_each_found_through_churn_and_forgotten)
{ ClGlMirrors        mirrors;
  constexpr std::uint32_t pool = 700u;
  static_assert(pool % 7u == 0u);
  for (std::uint32_t k = 0u; k < pool; k++) {
      CHECK(mirrors.remember(mirror_of(k)));
      CHECK(mirrors.invariants_hold());
  }
  CHECK(all_found(mirrors, 0u, pool));
  /* Every seventh let go, out of order, and one new made for each. */
  std::uint32_t made = pool;
  for (std::uint32_t step = 0u; step < pool / 7u; step++) {
      const std::uint32_t k = (step * 37u) % (pool / 7u) * 7u;
      GlMirror            gone;
      CHECK(mirrors.forget(scattered(k), gone));
      CHECK_EQ(gone.name(), k + 1u);
      CHECK(mirrors.find(scattered(k)) == nullptr);
      CHECK(mirrors.invariants_hold());
      CHECK(mirrors.remember(mirror_of(made++)));
      CHECK(mirrors.invariants_hold());
  }
  CHECK_EQ(mirrors.count(), pool);
  CHECK(all_found(mirrors, pool, made));
  bool kept = true;
  for (std::uint32_t k = 0u; k < pool; k++) kept = kept && (mirrors.find(scattered(k)) != nullptr) == (k % 7u != 0u);
  CHECK(kept);
  /* All of them, from the first made to the last. */
  for (std::uint32_t k = 0u; k < made; k++) {
      GlMirror gone;
      if (k < pool && k % 7u == 0u) continue;
      CHECK(mirrors.forget(scattered(k), gone));
      CHECK_EQ(gone.name(), k + 1u);
      CHECK(mirrors.invariants_hold());
  }
  CHECK_EQ(mirrors.count(), 0u); }

/* No memory until a mirror is held; kept while the context makes and lets go
 * of them, so a pool that empties and fills maps nothing again; given back by
 * forget_all, and mapped afresh by the next. */
TEST(cl_gl_mirrors, the_table_is_mapped_at_the_first_mirror_and_given_back_at_the_end)
{ ClGlMirrors mirrors;
  CHECK(!ClGlMirrorsProbe::mapped(mirrors));
  CHECK(!mirrors.remember(GlMirror()));
  CHECK(!ClGlMirrorsProbe::mapped(mirrors));
  GlMirror gone;
  CHECK(!mirrors.forget(scattered(0u), gone));
  CHECK(!mirrors.forget_any(gone));
  CHECK(mirrors.remember(mirror_of(0u)));
  CHECK(ClGlMirrorsProbe::mapped(mirrors));
  CHECK(mirrors.forget_any(gone));
  CHECK(ClGlMirrorsProbe::mapped(mirrors));
  CHECK(mirrors.remember(mirror_of(1u)));
  CHECK(mirrors.remember(mirror_of(2u)));
  mirrors.forget_all();
  CHECK(!ClGlMirrorsProbe::mapped(mirrors));
  CHECK_EQ(mirrors.count(), 0u);
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.find(scattered(1u)) == nullptr);
  CHECK(mirrors.remember(mirror_of(1u)));
  CHECK(mirrors.invariants_hold());
  CHECK_EQ(mirrors.find(scattered(1u))->name(), 2u);
  /* Twice is once. */
  mirrors.forget_all();
  mirrors.forget_all();
  CHECK(mirrors.invariants_hold()); }

/* The address space closed to new mappings for the one call: the mirror is
 * refused, nothing is held, and it is held once memory can be had again. */
TEST(cl_gl_mirrors, a_table_no_memory_can_be_had_for_refuses_the_mirror)
{ ClGlMirrors mirrors;
  rlimit      was = {};
  CHECK_EQ(getrlimit(RLIMIT_AS, &was), 0);
  rlimit closed = was;
  closed.rlim_cur = 0u;
  CHECK_EQ(setrlimit(RLIMIT_AS, &closed), 0);
  const bool remembered = mirrors.remember(mirror_of(0u));
  CHECK_EQ(setrlimit(RLIMIT_AS, &was), 0);
  CHECK(!remembered);
  CHECK(!ClGlMirrorsProbe::mapped(mirrors));
  CHECK_EQ(mirrors.count(), 0u);
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.remember(mirror_of(0u)));
  CHECK_EQ(mirrors.count(), 1u); }

TEST(cl_gl_mirrors, the_invariant_fails_each_way_the_table_can_be_wrong)
{ /* Held with no table. */
  { ClGlMirrors mirrors;
    ClGlMirrorsProbe::count(mirrors, 1u);
    CHECK(!mirrors.invariants_hold()); }
  /* More than the table. */
  { ClGlMirrors mirrors;
    CHECK(mirrors.remember(mirror_of(0u)));
    ClGlMirrorsProbe::count(mirrors, ClGlMirrors::max_mirrors + 1u);
    CHECK(!mirrors.invariants_hold()); }
  /* A held slot of nothing. */
  { ClGlMirrors mirrors;
    CHECK(mirrors.remember(mirror_of(0u)));
    ClGlMirrorsProbe::put(mirrors, 0u, GlMirror());
    CHECK(!mirrors.invariants_hold()); }
  /* One mem twice: the second is found at the first. */
  { ClGlMirrors mirrors;
    CHECK(mirrors.remember(mirror_of(0u)));
    CHECK(mirrors.remember(mirror_of(1u)));
    ClGlMirrorsProbe::put(mirrors, 1u, GlMirror(scattered(0u), 9u, 64u, true));
    CHECK(!mirrors.invariants_hold()); }
  /* A held slot the index has lost. */
  { ClGlMirrors mirrors;
    CHECK(mirrors.remember(mirror_of(0u)));
    ClGlMirrorsProbe::index(mirrors, ClGlMirrorsProbe::bucket_of(mirrors, scattered(0u)), 0u);
    CHECK(!mirrors.invariants_hold()); }
  /* An entry for a slot past the held. */
  { ClGlMirrors mirrors;
    CHECK(mirrors.remember(mirror_of(0u)));
    ClGlMirrorsProbe::index(mirrors, ClGlMirrorsProbe::empty_bucket(mirrors), 2u);
    CHECK(!mirrors.invariants_hold()); }
  /* A held slot indexed twice. */
  { ClGlMirrors mirrors;
    CHECK(mirrors.remember(mirror_of(0u)));
    CHECK(mirrors.remember(mirror_of(1u)));
    const std::uint32_t bucket = ClGlMirrorsProbe::empty_bucket(mirrors);
    ClGlMirrorsProbe::index(mirrors, bucket, 1u);
    CHECK_EQ(ClGlMirrorsProbe::entry(mirrors, bucket), 1u);
    CHECK(!mirrors.invariants_hold()); } }
