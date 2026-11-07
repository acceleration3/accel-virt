#include "core/support/status.hpp"
#include "executor/opengl/gl_texture_targets.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::GlTextureTargets;
using av::MockPlatform;
using av::Status;

TEST(gl_texture_targets, remembers_the_first_target_a_texture_was_made_or_bound_with)
{ MockPlatform platform;
  GlTextureTargets targets(platform);
  CHECK_EQ(targets.target_of(3u), 0u);
  CHECK_STATUS(targets.note(3u, 0x0DE1u), Status::ok);
  CHECK_STATUS(targets.note(3u, 0x8513u), Status::ok);
  CHECK_EQ(targets.target_of(3u), 0x0DE1u);
  CHECK_STATUS(targets.note(4u, 0x806Fu), Status::ok);
  CHECK_EQ(targets.target_of(4u), 0x806Fu);
  CHECK_EQ(targets.count(), 2u);
  CHECK(targets.invariants_hold()); }

TEST(gl_texture_targets, refuses_texture_zero_and_target_zero)
{ MockPlatform platform;
  GlTextureTargets targets(platform);
  CHECK_STATUS(targets.note(0u, 0x0DE1u), Status::invalid_parameter);
  CHECK_STATUS(targets.note(3u, 0u), Status::invalid_parameter);
  CHECK_EQ(targets.count(), 0u); }

TEST(gl_texture_targets, forgets_a_deleted_texture_so_a_reused_name_takes_a_new_target)
{ MockPlatform platform;
  GlTextureTargets targets(platform);
  CHECK_STATUS(targets.note(3u, 0x0DE1u), Status::ok);
  targets.forget(3u);
  targets.forget(3u);
  CHECK_EQ(targets.target_of(3u), 0u);
  CHECK_STATUS(targets.note(3u, 0x8513u), Status::ok);
  CHECK_EQ(targets.target_of(3u), 0x8513u);
  CHECK(targets.invariants_hold()); }
