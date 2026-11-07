#include "core/support/status.hpp"
#include "executor/opengl/gl_memory_group.hpp"
#include "executor/opengl/gl_memory_objects.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "executor/shim/gl_dma_buf_images.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/slice_progress.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <atomic>
#include <cstdint>
#include <pthread.h>
#include <time.h>

namespace av {
struct GlDmaBufImagesProbe {
    static void install(GlDmaBufImages &images, void *(*create)(void *, void *, unsigned, void *, const int *),
                        unsigned (*destroy)(void *, void *), void (*target)(unsigned, void *, const int *))
    {
        images.create_image_ = create;
        images.destroy_image_ = destroy;
        images.target_storage_ = target;
        images.display_ = &images;
    }
    static void uninstall(GlDmaBufImages &images)
    {
        images.display_ = nullptr;
        images.images_.clear();
    }
};
}

using av::GlDmaBufImages;
using av::GlDmaBufImagesProbe;
using av::GlHostImport;
using av::GlMemoryGroup;
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

TEST(gl_memory_objects, a_memory_one_session_made_is_usable_by_every_session_of_its_share_group)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryGroup group;
  GlMemoryObjects maker(platform, progress);
  GlMemoryObjects user(platform, progress);
  CHECK(maker.join(group));
  CHECK(user.join(group));
  CHECK_STATUS(maker.add(7u, 0x4000u), Status::ok);
  CHECK(user.holds(7u));
  CHECK_STATUS(user.admit(1u, 7u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(user.place(1u, 7u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(user.place_texture(2u, 7u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(user.inherit_texture(3u, 2u), Status::ok);
  CHECK_STATUS(user.admit_texture(4u, 7u, 0x4000u, 0x100u), Status::invalid_parameter);
  CHECK_EQ(maker.placed() + maker.textured(), 3u);
  CHECK(maker.invariants_hold());
  CHECK(user.invariants_hold());
  user.leave(1u);
  user.leave_texture(3u);
  maker.leave_texture(2u);
  CHECK_EQ(maker.placed() + maker.textured(), 0u);
  CHECK(maker.invariants_hold()); }

TEST(gl_memory_objects, a_session_outside_the_group_does_not_see_the_group_s_memory)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryGroup group;
  GlMemoryObjects maker(platform, progress);
  GlMemoryObjects stranger(platform, progress);
  CHECK(maker.join(group));
  CHECK_STATUS(maker.add(7u, 0x4000u), Status::ok);
  CHECK(!stranger.holds(7u));
  CHECK_STATUS(stranger.place_texture(1u, 7u, 0u, 16u), Status::invalid_parameter);
  GlMemoryObjects late(platform, progress);
  CHECK(late.join(group));
  CHECK(late.holds(7u));
  late.part();
  CHECK(!late.holds(7u));
  CHECK_EQ(group.members(), 1u); }

TEST(gl_memory_objects, a_memory_deleted_by_another_session_is_reclaimed_once_its_last_texture_leaves)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlHostImport import;
  GlMemoryGroup group;
  GlMemoryObjects maker(platform, progress);
  GlMemoryObjects user(platform, progress);
  CHECK(maker.join(group));
  CHECK(user.join(group));
  CHECK_STATUS(maker.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(user.place_texture(1u, 7u, 0u, 0x100u), Status::ok);
  user.retire(7u);
  user.reclaim(import);
  CHECK(maker.holds(7u));
  progress.reached_ = 5u;
  user.leave_texture(1u);
  user.reclaim(import);
  CHECK(!maker.holds(7u));
  CHECK_EQ(maker.count(), 0u); }

TEST(gl_memory_objects, a_texture_moved_to_another_memory_of_the_group_leaves_the_first)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryGroup group;
  GlMemoryObjects first(platform, progress);
  GlMemoryObjects second(platform, progress);
  CHECK(first.join(group));
  CHECK(second.join(group));
  CHECK_STATUS(first.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(second.add(8u, 0x4000u), Status::ok);
  CHECK_STATUS(second.place_texture(1u, 7u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(first.place_texture(1u, 8u, 0u, 0x100u), Status::ok);
  CHECK_EQ(first.textured(), 0u);
  CHECK_EQ(second.textured(), 1u);
  CHECK(first.invariants_hold());
  CHECK(second.invariants_hold());
  CHECK_STATUS(first.place(2u, 7u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(second.place(2u, 8u, 0u, 0x100u), Status::ok);
  CHECK_EQ(first.placed(), 0u);
  CHECK_EQ(second.placed(), 1u);
  CHECK_STATUS(second.place_texture(5u, 8u, 0u, 0x100u), Status::ok);
  CHECK_STATUS(first.place_texture(6u, 7u, 0u, 0x100u), Status::ok);
  CHECK_EQ(second.textured(), 2u);
  CHECK_STATUS(first.inherit_texture(5u, 6u), Status::ok);
  CHECK_EQ(second.textured(), 1u);
  CHECK_EQ(first.textured(), 2u);
  CHECK(first.invariants_hold());
  CHECK(second.invariants_hold()); }

TEST(gl_memory_objects, a_session_leaving_the_group_destroys_only_its_own_memory)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlHostImport import;
  GlMemoryGroup group;
  GlMemoryObjects leaving(platform, progress);
  GlMemoryObjects staying(platform, progress);
  CHECK(leaving.join(group));
  CHECK(staying.join(group));
  CHECK_STATUS(leaving.add(7u, 0x4000u), Status::ok);
  CHECK_STATUS(staying.add(8u, 0x4000u), Status::ok);
  leaving.destroy_all(import);
  leaving.part();
  CHECK(!staying.holds(7u));
  CHECK(staying.holds(8u));
  CHECK_EQ(group.members(), 1u); }

namespace {

struct Placing {
    GlMemoryObjects   *memories;
    std::atomic<bool>  done;
};

void *place_one(void *context)
{
    Placing &placing = *static_cast<Placing *>(context);
    static_cast<void>(placing.memories->place_texture(1u, 7u, 0u, 16u));
    placing.done.store(true);
    return nullptr;
}

}

TEST(gl_memory_objects, a_grouped_operation_waits_for_the_group_lock)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryGroup group;
  GlMemoryObjects memories(platform, progress);
  CHECK(memories.join(group));
  CHECK_STATUS(memories.add(7u, 0x4000u), Status::ok);
  Placing placing = {&memories, false};
  group.lock();
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, place_one, &placing), 0);
  const timespec pause = {0, 100 * 1000 * 1000};
  nanosleep(&pause, nullptr);
  const bool finished_early = placing.done.load();
  group.unlock();
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(!finished_early);
  CHECK(placing.done.load());
  CHECK_EQ(memories.textured(), 1u); }

TEST(gl_memory_objects, a_group_holds_at_most_its_members_and_a_member_joins_one_group_at_a_time)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryGroup one;
  GlMemoryGroup other;
  GlMemoryObjects memories(platform, progress);
  CHECK(memories.join(one));
  CHECK(memories.join(one));
  CHECK_EQ(one.members(), 1u);
  CHECK(memories.join(other));
  CHECK_EQ(one.members(), 0u);
  CHECK_EQ(other.members(), 1u); }

namespace {

int      image_sentinel = 0;
unsigned images_destroyed = 0u;

void *fake_image_create(void *, void *, unsigned, void *, const int *) { return &image_sentinel; }

unsigned fake_image_destroy(void *, void *)
{
    images_destroyed++;
    return 1u;
}

void fake_image_target(unsigned, void *, const int *) {}

struct Adding {
    GlMemoryObjects   *memories;
    std::atomic<bool>  done;
};

void *add_one(void *context)
{
    Adding &adding = *static_cast<Adding *>(context);
    static_cast<void>(adding.memories->add(8u, 0x4000u));
    adding.done.store(true);
    return nullptr;
}

}

TEST(gl_memory_objects, adding_a_memory_waits_for_the_group_lock)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlMemoryGroup group;
  GlMemoryObjects memories(platform, progress);
  CHECK(memories.join(group));
  Adding adding = {&memories, false};
  group.lock();
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, add_one, &adding), 0);
  const timespec pause = {0, 100 * 1000 * 1000};
  nanosleep(&pause, nullptr);
  const bool finished_early = adding.done.load();
  group.unlock();
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(!finished_early);
  CHECK(adding.done.load());
  CHECK(memories.holds(8u)); }

TEST(gl_memory_objects, a_memory_reclaimed_by_another_session_forgets_its_image_in_the_owner_s_table)
{ MockPlatform platform;
  ScriptedProgress progress;
  GlDmaBufImages maker_images(platform);
  GlDmaBufImages user_images(platform);
  GlHostImport   maker_import;
  GlHostImport   user_import;
  maker_import.attach(maker_images);
  user_import.attach(user_images);
  GlDmaBufImagesProbe::install(maker_images, fake_image_create, fake_image_destroy, fake_image_target);
  GlDmaBufImagesProbe::install(user_images, fake_image_create, fake_image_destroy, fake_image_target);
  CHECK(maker_images.import(7u, 3, av::SharedImageShape(16u, 16u, 28u, 64u)));
  CHECK(user_images.import(7u, 3, av::SharedImageShape(16u, 16u, 28u, 64u)));
  images_destroyed = 0u;
  GlMemoryGroup group;
  GlMemoryObjects maker(platform, progress);
  GlMemoryObjects user(platform, progress);
  CHECK(maker.join(group, &maker_import));
  CHECK(user.join(group, &user_import));
  CHECK_STATUS(maker.add(7u, 0x4000u), Status::ok);
  user.retire(7u);
  progress.reached_ = 5u;
  user.reclaim(user_import);
  CHECK(!maker.holds(7u));
  CHECK_EQ(maker_images.count(), 0u);
  CHECK_EQ(user_images.count(), 1u);
  CHECK_EQ(images_destroyed, 1u);
  user.part();
  maker.part();
  GlDmaBufImagesProbe::uninstall(maker_images);
  GlDmaBufImagesProbe::uninstall(user_images); }
