#include "core/support/status.hpp"
#include "executor/opengl/gl_memory_objects.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/slice_progress.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::GlHostImport;
using av::GlMemoryObjects;
using av::MockPlatform;
using av::SliceProgress;
using av::Status;

namespace {

class ScriptedProgress final : public SliceProgress {
public:
    std::uint64_t next() const override { return next_; }
    std::uint64_t reached() const override { return reached_; }

    std::uint64_t next_ = 1u;
    std::uint64_t reached_ = 0u;
};

}

TEST(gl_memory_objects, holds_a_named_memory_of_a_size_and_refuses_a_name_twice_or_nothing)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK(memories.holds(7u));
  CHECK(!memories.holds(8u));
  CHECK_EQ(memories.count(), 1u);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::invalid_parameter);
  CHECK_STATUS(memories.add(0u, 0x4000u), Status::invalid_parameter);
  CHECK_STATUS(memories.add(9u, 0u), Status::invalid_parameter);
  CHECK_EQ(memories.count(), 1u);
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, admits_and_places_a_buffer_only_inside_the_memory)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.admit(1u, 7u, 0u, 0x4000u), Status::ok);
  CHECK_EQ(memories.placed(), 0u);
  CHECK_STATUS(memories.admit(0u, 7u, 0u, 16u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit(1u, 8u, 0u, 16u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit(1u, 7u, 0u, 0u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit(1u, 7u, 0x4001u, 1u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit(1u, 7u, 0x3000u, 0x1001u), Status::invalid_parameter);
  CHECK_STATUS(memories.place(1u, 7u, 0x1000u, 0x1000u), Status::ok);
  CHECK_EQ(memories.placed(), 1u);
  CHECK_STATUS(memories.place(1u, 7u, 0x2000u, 0x1000u), Status::ok);
  CHECK_EQ(memories.placed(), 1u);
  CHECK_STATUS(memories.place(2u, 7u, 0u, 0x100u), Status::ok);
  CHECK_EQ(memories.placed(), 2u);
  CHECK(memories.invariants_hold());
  memories.leave(1u);
  memories.leave(1u);
  CHECK_EQ(memories.placed(), 1u);
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, a_retired_memory_is_kept_while_placed_or_until_the_gpu_passes_its_mark)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlHostImport import;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.place(1u, 7u, 0u, 0x100u), Status::ok);
  progress.next_ = 3u;
  memories.retire(7u);
  memories.retire(7u);
  memories.retire(99u);
  CHECK_STATUS(memories.admit(2u, 7u, 0u, 16u), Status::invalid_parameter);
  progress.reached_ = 5u;
  memories.reclaim(import);
  CHECK(memories.holds(7u));
  memories.leave(1u);
  progress.reached_ = 2u;
  memories.reclaim(import);
  CHECK(memories.holds(7u));
  progress.reached_ = 3u;
  memories.reclaim(import);
  CHECK(!memories.holds(7u));
  CHECK_EQ(memories.count(), 0u);
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, destroys_everything_it_holds_with_its_placements)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlHostImport import;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.add(8u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.place(1u, 7u, 0u, 0x100u), Status::ok);
  memories.destroy_all(import);
  CHECK_EQ(memories.count(), 0u);
  CHECK_EQ(memories.placed(), 0u);
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, admits_and_places_a_texture_only_inside_the_memory_and_counts_it_apart_from_buffers)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.admit_texture(1u, 7u, 0u, 0x4000u), Status::ok);
  CHECK_EQ(memories.textured(), 0u);
  CHECK_STATUS(memories.admit_texture(0u, 7u, 0u, 16u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit_texture(1u, 8u, 0u, 16u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit_texture(1u, 7u, 0u, 0u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit_texture(1u, 7u, 0x4001u, 1u), Status::invalid_parameter);
  CHECK_STATUS(memories.admit_texture(1u, 7u, 0x3000u, 0x1001u), Status::invalid_parameter);
  CHECK_STATUS(memories.place_texture(1u, 7u, 0x1000u, 0x1000u), Status::ok);
  CHECK_STATUS(memories.place_texture(1u, 7u, 0x2000u, 0x1000u), Status::ok);
  CHECK_EQ(memories.textured(), 1u);
  CHECK_STATUS(memories.place_texture(2u, 7u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(memories.place(1u, 7u, 0u, 0x100u), Status::ok);
  CHECK_EQ(memories.textured(), 2u);
  CHECK_EQ(memories.placed(), 1u);
  CHECK(memories.invariants_hold());
  memories.leave_texture(1u);
  memories.leave_texture(1u);
  CHECK_EQ(memories.textured(), 1u);
  CHECK_EQ(memories.placed(), 1u);
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, a_view_shares_the_memory_of_its_original_and_only_a_placed_original_has_views)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.inherit_texture(2u, 1u), Status::not_found);
  CHECK_STATUS(memories.place_texture(1u, 7u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(memories.inherit_texture(2u, 1u), Status::ok);
  CHECK_STATUS(memories.inherit_texture(0u, 1u), Status::invalid_parameter);
  CHECK_STATUS(memories.inherit_texture(1u, 1u), Status::invalid_parameter);
  CHECK_STATUS(memories.inherit_texture(2u, 1u), Status::ok);
  CHECK_EQ(memories.textured(), 2u);
  memories.leave_texture(1u);
  CHECK_EQ(memories.textured(), 1u);
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, a_retired_memory_is_kept_while_a_texture_stands_on_it_and_until_the_gpu_passes_the_last_use)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlHostImport import;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.place_texture(1u, 7u, 0u, 0x100u), Status::ok);
  progress.next_ = 3u;
  memories.retire(7u);
  CHECK_STATUS(memories.admit_texture(2u, 7u, 0u, 16u), Status::invalid_parameter);
  progress.reached_ = 9u;
  memories.reclaim(import);
  CHECK(memories.holds(7u));
  progress.next_ = 11u;
  memories.leave_texture(1u);
  progress.reached_ = 10u;
  memories.reclaim(import);
  CHECK(memories.holds(7u));
  progress.reached_ = 11u;
  memories.reclaim(import);
  CHECK(!memories.holds(7u));
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, a_buffer_leaving_a_retired_memory_moves_its_mark_past_the_work_that_used_it)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlHostImport import;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.place(1u, 7u, 0u, 0x100u), Status::ok);
  progress.next_ = 2u;
  memories.retire(7u);
  progress.next_ = 8u;
  memories.leave(1u);
  progress.reached_ = 7u;
  memories.reclaim(import);
  CHECK(memories.holds(7u));
  progress.reached_ = 8u;
  memories.reclaim(import);
  CHECK(!memories.holds(7u)); }

TEST(gl_memory_objects, destroying_everything_forgets_the_textures_too)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlHostImport import;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(memories.place_texture(1u, 7u, 0u, 0x100u), Status::ok);
  memories.destroy_all(import);
  CHECK_EQ(memories.textured(), 0u);
  CHECK_EQ(memories.count(), 0u);
  CHECK(memories.invariants_hold()); }

TEST(gl_memory_objects, buffers_and_textures_together_stop_at_the_most_placements)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryObjects memories(platform, progress);
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  const std::uint32_t buffers = GlMemoryObjects::most_placements / 2u;
  for (std::uint32_t name = 1u; name <= buffers; name++) CHECK_STATUS(memories.place(name, 7u, 0u, 16u), Status::ok);
  for (std::uint32_t name = 1u; name <= GlMemoryObjects::most_placements - buffers; name++)
      CHECK_STATUS(memories.place_texture(name, 7u, 0u, 16u), Status::ok);
  CHECK_EQ(memories.placed() + memories.textured(), GlMemoryObjects::most_placements);
  CHECK_STATUS(memories.admit_texture(0xFFFFu, 7u, 0u, 16u), Status::no_memory);
  CHECK_STATUS(memories.admit(0xFFFFu, 7u, 0u, 16u), Status::no_memory);
  CHECK_STATUS(memories.inherit_texture(0xFFFFu, 1u), Status::no_memory);
  memories.leave_texture(1u);
  CHECK_STATUS(memories.admit(0xFFFFu, 7u, 0u, 16u), Status::ok);
  CHECK(memories.invariants_hold()); }
