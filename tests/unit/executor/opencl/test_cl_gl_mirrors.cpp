/* The cl_mems standing in for GL objects (ClGlMirrors): each found by its
 * cl_mem until forgotten, which hands back what it was; everything the table
 * refuses, and the invariant broken each way it can be. */
#include "executor/opencl/cl_gl_mirrors.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

struct ClGlMirrorsProbe {
    static void count(ClGlMirrors &mirrors, std::uint32_t count) { mirrors.count_ = count; }
    static void put(ClGlMirrors &mirrors, std::uint32_t slot, const GlMirror &mirror) { mirrors.mirrors_[slot] = mirror; }
};

}  // namespace av

using av::ClGlMirrors;
using av::ClGlMirrorsProbe;
using av::GlMirror;

namespace {

const void *mem_at(std::uintptr_t at) { return reinterpret_cast<const void *>(at); }
GlMirror    buffer_at(std::uintptr_t at, std::uint32_t name) { return GlMirror(mem_at(at), name, 64u, true); }

}  // namespace

TEST(cl_gl_mirrors, a_remembered_mirror_is_found_by_its_mem_until_it_is_forgotten)
{ ClGlMirrors mirrors;
  CHECK(mirrors.invariants_hold());
  CHECK(mirrors.find(mem_at(0x1000u)) == nullptr);
  CHECK(mirrors.remember(buffer_at(0x1000u, 7u)));
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
  CHECK_EQ(mirrors.count(), 1u); }

TEST(cl_gl_mirrors, a_mirror_of_nothing_a_mem_held_twice_and_a_full_table_are_refused)
{ ClGlMirrors mirrors;
  CHECK(!mirrors.remember(GlMirror()));
  CHECK(!mirrors.remember(GlMirror(nullptr, 7u, 64u, true)));
  CHECK(mirrors.remember(buffer_at(0x1000u, 7u)));
  CHECK(!mirrors.remember(buffer_at(0x1000u, 9u)));
  CHECK_EQ(mirrors.find(mem_at(0x1000u))->name(), 7u);
  for (std::uint32_t slot = 1u; slot < ClGlMirrors::max_mirrors; slot++)
      CHECK(mirrors.remember(buffer_at(0x1000u + slot * 0x10u, slot + 7u)));
  CHECK_EQ(mirrors.count(), ClGlMirrors::max_mirrors);
  CHECK(!mirrors.remember(buffer_at(0xF0000u, 1u)));
  CHECK(mirrors.invariants_hold());
  /* Forgotten one at a time to the last, then nothing. */
  GlMirror      gone;
  std::uint32_t forgotten = 0u;
  while (mirrors.forget_any(gone)) {
      forgotten++;
      CHECK(gone.described());
      CHECK(mirrors.invariants_hold());
  }
  CHECK_EQ(forgotten, ClGlMirrors::max_mirrors);
  CHECK_EQ(mirrors.count(), 0u);
  CHECK(!mirrors.forget_any(gone)); }

TEST(cl_gl_mirrors, the_invariant_fails_each_way_the_table_can_be_wrong)
{ { ClGlMirrors mirrors;
    ClGlMirrorsProbe::count(mirrors, ClGlMirrors::max_mirrors + 1u);
    CHECK(!mirrors.invariants_hold()); }
  /* A held slot of nothing. */
  { ClGlMirrors mirrors;
    ClGlMirrorsProbe::count(mirrors, 1u);
    CHECK(!mirrors.invariants_hold()); }
  /* One mem twice. */
  { ClGlMirrors mirrors;
    CHECK(mirrors.remember(buffer_at(0x1000u, 7u)));
    ClGlMirrorsProbe::put(mirrors, 1u, buffer_at(0x1000u, 8u));
    ClGlMirrorsProbe::count(mirrors, 2u);
    CHECK(!mirrors.invariants_hold()); }
  /* Something past the held. */
  { ClGlMirrors mirrors;
    ClGlMirrorsProbe::put(mirrors, 3u, buffer_at(0x1000u, 7u));
    CHECK(!mirrors.invariants_hold()); } }
