#include "executor/opengl/gl_error_queue.hpp"
#include "executor/opengl/gl_vram_guard.hpp"
#include "executor/vram_ledger.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/failing_blocks.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/submission/gl_error_word.hpp"

#include <cstdint>
#include <initializer_list>

using av::GlVramGuard;
using namespace av::glproto;

namespace {

constexpr std::uint64_t mib = 1ull << 20;
constexpr std::uint32_t array_buffer = 0x8892u, array_buffer_binding = 0x8894u;
constexpr std::uint32_t texture_2d = 0x0DE1u, texture_2d_binding = 0x8069u;
constexpr std::uint32_t rgba8 = 0x8058u;
constexpr std::uint32_t out_of_memory_kind = 5u;

struct Bindings {
    std::uint32_t binding[8] = {};
    std::int32_t  name[8] = {};
    std::uint32_t used = 0u;
};

Bindings bindings;

void bind(std::uint32_t binding, std::int32_t name)
{ for (std::uint32_t i = 0u; i < bindings.used; i++)
      if (bindings.binding[i] == binding) { bindings.name[i] = name; return; }
  bindings.binding[bindings.used] = binding;
  bindings.name[bindings.used++] = name; }

void fake_get_integerv(GLenum pname, GLint *data)
{ *data = 0;
  for (std::uint32_t i = 0u; i < bindings.used; i++)
      if (bindings.binding[i] == pname) *data = bindings.name[i]; }

class Rig {
public:
    Rig() : blocks_(), gl_(), word_(), queue_(gl_, word_), ledger_(), guard_(blocks_, gl_, queue_)
    { bindings = Bindings{};
      gl_.glGetIntegerv = fake_get_integerv;
      ledger_.learn(2048 * mib);
      guard_.bind(&ledger_);
      guard_.install(); }

    bool run(std::uint32_t command, std::initializer_list<std::uint64_t> args)
    { std::uint64_t padded[10] = {};
      std::uint32_t at = 0u;
      for (const std::uint64_t value : args) padded[at++] = value;
      return gl_.av_vram_hook(gl_.av_vram_user, command, padded); }

    av::FailingBlocks &blocks() { return blocks_; }
    RealGl            &gl() { return gl_; }
    av::GlErrorWord   &word() { return word_; }
    av::VramLedger    &ledger() { return ledger_; }
    GlVramGuard       &guard() { return guard_; }

private:
    av::FailingBlocks blocks_;
    RealGl            gl_;
    av::GlErrorWord   word_;
    av::GlErrorQueue  queue_;
    av::VramLedger    ledger_;
    GlVramGuard       guard_;
};

}

TEST(gl_vram_guard, installing_puts_the_guard_in_front_of_every_allocating_command)
{ Rig rig;
  CHECK(rig.gl().av_vram_hook != nullptr);
  CHECK(rig.gl().av_vram_user == &rig.guard());
  CHECK_EQ(kVramCommands, 31u); }

TEST(gl_vram_guard, a_buffer_that_fits_is_counted_and_one_past_the_cap_is_out_of_memory)
{ Rig rig;
  bind(array_buffer_binding, 3);
  CHECK(rig.run(kVram_glBufferData, {array_buffer, 600 * mib}));
  CHECK_EQ(rig.ledger().used(), 600 * mib);
  bind(array_buffer_binding, 4);
  CHECK(!rig.run(kVram_glBufferData, {array_buffer, 500 * mib}));
  CHECK_EQ(rig.word().count(out_of_memory_kind), 1u);
  CHECK_EQ(rig.ledger().used(), 600 * mib);
  CHECK(rig.run(kVram_glBufferData, {array_buffer, 424 * mib}));
  CHECK_EQ(rig.ledger().used(), 1024 * mib);
  CHECK(rig.guard().invariants_hold()); }

TEST(gl_vram_guard, respecifying_a_buffer_replaces_its_size_in_both_directions)
{ Rig rig;
  bind(array_buffer_binding, 3);
  CHECK(rig.run(kVram_glBufferData, {array_buffer, 800 * mib}));
  CHECK(rig.run(kVram_glBufferData, {array_buffer, 100 * mib}));
  CHECK_EQ(rig.ledger().used(), 100 * mib);
  CHECK(rig.run(kVram_glBufferData, {array_buffer, 1000 * mib}));
  CHECK_EQ(rig.ledger().used(), 1000 * mib);
  CHECK(!rig.run(kVram_glBufferData, {array_buffer, 1025 * mib}));
  CHECK_EQ(rig.ledger().used(), 1000 * mib); }

TEST(gl_vram_guard, a_named_buffer_and_its_storage_variants_count_by_name)
{ Rig rig;
  CHECK(rig.run(kVram_glNamedBufferData, {7, 100 * mib}));
  CHECK(rig.run(kVram_glNamedBufferStorage, {8, 200 * mib}));
  CHECK(rig.run(kVram_glNamedBufferDataEXT, {9, 50 * mib}));
  CHECK(rig.run(kVram_glNamedBufferStorageEXT, {10, 50 * mib}));
  CHECK_EQ(rig.ledger().used(), 400 * mib);
  CHECK(!rig.run(kVram_glNamedBufferStorage, {11, 700 * mib}));
  const std::uint32_t gone[] = {7, 8};
  CHECK(rig.run(kVram_glDeleteBuffers, {2, reinterpret_cast<std::uintptr_t>(gone)}));
  CHECK_EQ(rig.ledger().used(), 100 * mib); }

TEST(gl_vram_guard, a_buffer_target_nothing_is_bound_to_or_a_negative_size_is_the_drivers_to_judge)
{ Rig fresh;
  CHECK(fresh.run(kVram_glBufferData, {array_buffer, 5000 * mib}));
  bind(array_buffer_binding, 1);
  CHECK(fresh.run(kVram_glBufferData, {array_buffer, static_cast<std::uint64_t>(-1)}));
  CHECK(fresh.run(kVram_glBufferData, {0x1234u, 5000 * mib}));
  CHECK_EQ(fresh.ledger().used(), 0u);
  CHECK_EQ(fresh.word().count(out_of_memory_kind), 0u); }

TEST(gl_vram_guard, a_texture_level_counts_its_texels_and_a_second_specification_replaces_it)
{ Rig rig;
  bind(texture_2d_binding, 5);
  CHECK(rig.run(kVram_glTexImage2D, {texture_2d, 0, rgba8, 1024, 1024, 0, 0, 0, 0}));
  CHECK_EQ(rig.ledger().used(), 4 * mib);
  CHECK(rig.run(kVram_glTexImage2D, {texture_2d, 1, rgba8, 512, 512, 0, 0, 0, 0}));
  CHECK_EQ(rig.ledger().used(), 5 * mib);
  CHECK(rig.run(kVram_glTexImage2D, {texture_2d, 0, rgba8, 512, 512, 0, 0, 0, 0}));
  CHECK_EQ(rig.ledger().used(), 2 * mib);
  const std::uint32_t gone[] = {5};
  CHECK(rig.run(kVram_glDeleteTextures, {1, reinterpret_cast<std::uintptr_t>(gone)}));
  CHECK_EQ(rig.ledger().used(), 0u);
  CHECK_EQ(rig.guard().objects(), 0u); }

TEST(gl_vram_guard, a_texture_that_does_not_fit_is_out_of_memory_and_leaves_the_books_alone)
{ Rig rig;
  bind(texture_2d_binding, 5);
  CHECK(!rig.run(kVram_glTexImage2D, {texture_2d, 0, rgba8, 20000, 20000, 0, 0, 0, 0}));
  CHECK_EQ(rig.word().count(out_of_memory_kind), 1u);
  CHECK_EQ(rig.ledger().used(), 0u);
  CHECK_EQ(rig.guard().objects(), 0u); }

TEST(gl_vram_guard, immutable_storage_counts_the_whole_mip_chain_once)
{ Rig rig;
  bind(texture_2d_binding, 6);
  CHECK(rig.run(kVram_glTexStorage2D, {texture_2d, 11, rgba8, 1024, 1024}));
  const std::uint64_t chain = rig.ledger().used();
  CHECK(chain > 4 * mib && chain < 6 * mib);
  CHECK(rig.run(kVram_glTextureStorage2D, {9, 1, rgba8, 1024, 1024}));
  CHECK_EQ(rig.ledger().used(), chain + 4 * mib); }

TEST(gl_vram_guard, a_multisampled_texture_costs_its_samples)
{ Rig rig;
  bind(0x8069u, 6);
  CHECK(rig.run(kVram_glTextureStorage2DMultisample, {3, 4, rgba8, 512, 512, 1}));
  CHECK_EQ(rig.ledger().used(), 4 * mib);
  bind(0x9104u, 7);
  CHECK(rig.run(kVram_glTexStorage2DMultisample, {0x9100u, 2, rgba8, 512, 512, 1}));
  CHECK_EQ(rig.ledger().used(), 6 * mib); }

TEST(gl_vram_guard, a_compressed_image_costs_the_size_the_guest_gave)
{ Rig rig;
  bind(texture_2d_binding, 6);
  CHECK(rig.run(kVram_glCompressedTexImage2D, {texture_2d, 0, 0x83F1u, 256, 256, 0, 32768, 0}));
  CHECK_EQ(rig.ledger().used(), 32768u);
  CHECK(rig.run(kVram_glCompressedTexImage2D, {texture_2d, 0, 0x83F1u, 256, 256, 0, 0, 0}));
  CHECK_EQ(rig.ledger().used(), 32768u); }

TEST(gl_vram_guard, a_renderbuffer_counts_by_name_or_by_binding_and_is_freed_by_deletion)
{ Rig rig;
  bind(0x8CA7u, 2);
  CHECK(rig.run(kVram_glRenderbufferStorage, {0x8D41u, rgba8, 1024, 1024}));
  CHECK(rig.run(kVram_glNamedRenderbufferStorageMultisample, {4, 4, rgba8, 512, 512}));
  CHECK_EQ(rig.ledger().used(), 8 * mib);
  CHECK(!rig.run(kVram_glNamedRenderbufferStorage, {5, rgba8, 20000, 20000}));
  const std::uint32_t gone[] = {2, 4};
  CHECK(rig.run(kVram_glDeleteRenderbuffers, {2, reinterpret_cast<std::uintptr_t>(gone)}));
  CHECK_EQ(rig.ledger().used(), 0u); }

TEST(gl_vram_guard, arguments_the_driver_would_call_invalid_are_passed_through_uncounted)
{ Rig rig;
  bind(texture_2d_binding, 5);
  CHECK(rig.run(kVram_glTexImage2D, {texture_2d, 0, rgba8, 0, 1024, 0, 0, 0, 0}));
  CHECK(rig.run(kVram_glTexStorage2D, {texture_2d, 0, rgba8, 1024, 1024}));
  CHECK(rig.run(kVram_glTexImage2D, {0x8063u, 0, rgba8, 99999, 99999, 0, 0, 0, 0}));
  CHECK(rig.run(kVram_glNamedRenderbufferStorage, {5, rgba8, static_cast<std::uint64_t>(-4), 100}));
  CHECK(rig.run(kVram_glDeleteTextures, {1, 0}));
  CHECK_EQ(rig.ledger().used(), 0u);
  CHECK_EQ(rig.word().count(out_of_memory_kind), 0u); }

TEST(gl_vram_guard, a_context_with_no_ledger_is_never_refused)
{ Rig rig;
  rig.guard().bind(nullptr);
  bind(array_buffer_binding, 3);
  CHECK(rig.run(kVram_glBufferData, {array_buffer, 100000 * mib}));
  CHECK_EQ(rig.guard().objects(), 0u); }

TEST(gl_vram_guard, an_object_that_cannot_be_recorded_is_refused_and_returns_its_bytes)
{ Rig rig;
  bind(array_buffer_binding, 3);
  rig.blocks().fail_from(rig.blocks().requests() + 1u);
  bool refused = false;
  for (std::uint32_t name = 1u; name < 200u && !refused; name++) {
      bind(array_buffer_binding, static_cast<std::int32_t>(name));
      refused = !rig.run(kVram_glBufferData, {array_buffer, mib});
  }
  CHECK(refused);
  CHECK(rig.guard().invariants_hold());
  CHECK_EQ(rig.ledger().used(), rig.guard().held()); }

TEST(gl_vram_guard, clearing_returns_everything_the_context_left_behind)
{ Rig rig;
  for (std::uint32_t name = 1u; name <= 6u; name++) CHECK(rig.run(kVram_glNamedBufferData, {name, 50 * mib}));
  CHECK_EQ(rig.ledger().used(), 300 * mib);
  rig.guard().clear();
  CHECK_EQ(rig.ledger().used(), 0u);
  CHECK_EQ(rig.guard().objects(), 0u); }
