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
