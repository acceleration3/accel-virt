#include "executor/d3d/d3d12_object_holds.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/teardown_budget.hpp"
#include "harness/executor/d3d12_session_loopback.hpp"
#include "harness/executor/fake_allocator.hpp"
#include "harness/executor/fake_command_list.hpp"
#include "harness/executor/fake_descriptor_heap.hpp"
#include "harness/executor/fake_device.hpp"
#include "harness/executor/fake_queue.hpp"
#include "harness/executor/fake_resource.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

#include <cstdint>

using av::D3dObjectKind;
using namespace av::d3d12proto;

namespace {

constexpr std::uint32_t list = 1u;
constexpr std::uint32_t other_list = 2u;
constexpr std::uint32_t allocator = 3u;
constexpr std::uint32_t target = 4u;
constexpr std::uint32_t source = 5u;
constexpr std::uint32_t queue_a = 6u;
constexpr std::uint32_t queue_b = 7u;
constexpr std::uint32_t heap = 8u;
constexpr std::uint32_t tiled = 9u;
constexpr std::uint32_t tiled_copy = 10u;
constexpr std::uint32_t render_heap = 11u;
constexpr std::uint64_t buffer_bytes = 1024u;
constexpr std::uint32_t device_hung = 0x887A0006u;
constexpr std::uint64_t quick = 20000000u;

D3D12_RESOURCE_DESC buffer_desc()
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = buffer_bytes;
    desc.Height = 1u;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.SampleDesc.Count = 1u;
    return desc;
}

class Scene {
public:
    Scene()
    {
        rig.place(device);
        dst.desc = buffer_desc();
        src.desc = buffer_desc();
        tiled_buffer.desc = buffer_desc();
        tiled_other.desc = buffer_desc();
        heap_object.desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_object.desc.NumDescriptors = 8u;
        heap_object.desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        rtv_heap.desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap.desc.NumDescriptors = 4u;
        insert(list, D3dObjectKind::list12, &commands);
        insert(other_list, D3dObjectKind::list12, &more_commands);
        insert(allocator, D3dObjectKind::allocator12, &alloc);
        insert(target, D3dObjectKind::resource12, &dst);
        insert(source, D3dObjectKind::resource12, &src);
        insert(queue_a, D3dObjectKind::queue12, &first_queue);
        insert(queue_b, D3dObjectKind::queue12, &second_queue);
        insert(heap, D3dObjectKind::descriptor_heap12, &heap_object);
        insert(render_heap, D3dObjectKind::descriptor_heap12, &rtv_heap);
        insert(tiled, D3dObjectKind::resource12, &tiled_buffer);
        insert(tiled_copy, D3dObjectKind::resource12, &tiled_other);
        CHECK(session().on_ResetCommandList12(list, allocator, 0u));
        CHECK(session().on_ResetCommandList12(other_list, allocator, 0u));
    }

    void insert(std::uint32_t id, D3dObjectKind kind, IUnknown *object)
    {
        CHECK_STATUS(rig.objects().insert(id, kind, object), av::Status::ok);
    }

    av::D3d12Session &session() { return rig.session(); }

    bool execute(std::uint32_t queue, std::uint32_t which)
    {
        const std::uint32_t lists[1] = {which};
        return session().on_ExecuteCommandLists12(queue, 1u, lists);
    }

    void reap()
    {
        session().end_batch();
    }

    av::FakeCommandList    commands, more_commands;
    av::FakeAllocator      alloc;
    av::FakeResource       dst, src, tiled_buffer, tiled_other;
    av::FakeDescriptorHeap heap_object, rtv_heap;
    av::FakeQueue          first_queue, second_queue;
    av::FakeDevice         device;
    av::D3d12SessionLoopback rig;
};

}

TEST(d3d12_session_execution, a_list_that_ran_keeps_what_it_copied_into_until_the_gpu_is_done_with_the_run)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  CHECK_EQ(scene.first_queue.executions, 1u);
  CHECK_EQ(scene.first_queue.signals, 1u);
  CHECK_EQ(scene.first_queue.signalled, 1u);
  CHECK(scene.session().on_Destroy12(target));
  CHECK(scene.session().on_Destroy12(source));
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK_EQ(scene.src.references(), 2u);
  scene.reap();
  CHECK_EQ(scene.dst.references(), 2u);
  scene.device.fences[0].completed = 1u;
  scene.reap();
  CHECK_EQ(scene.dst.references(), 0u);
  CHECK_EQ(scene.src.references(), 0u); }

TEST(d3d12_session_execution, a_list_that_was_reset_holds_an_object_again_when_it_records_it_again)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK(scene.session().on_Destroy12(target));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 0u); }

TEST(d3d12_session_execution, a_long_batch_looks_at_the_gpu_again_after_a_fixed_number_of_commands)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  CHECK(scene.session().on_Destroy12(target));
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  scene.session().end_batch();
  CHECK_EQ(scene.dst.references(), 2u);
  scene.device.fences[0].completed = 1u;
  std::uint32_t supported = 0u;
  for (std::uint32_t command = 0u; command < 255u; command++) CHECK(av_d3d12_QueryHeapSlices12(scene.rig, &supported));
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK(av_d3d12_QueryHeapSlices12(scene.rig, &supported));
  CHECK_EQ(scene.dst.references(), 0u); }

TEST(d3d12_session_execution, an_object_destroyed_while_its_list_runs_is_released_when_the_run_is_done_and_not_before)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  CHECK(scene.session().on_Destroy12(list));
  CHECK(scene.session().on_Destroy12(target));
  CHECK_EQ(scene.dst.references(), 2u);
  scene.device.fences[0].completed = 0u;
  scene.reap();
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK_EQ(scene.commands.references(), 1u);
  scene.device.fences[0].completed = 1u;
  scene.reap();
  CHECK_EQ(scene.dst.references(), 0u);
  CHECK_EQ(scene.commands.references(), 0u); }

TEST(d3d12_session_execution, an_object_stays_until_every_queue_is_past_the_marks_it_had_when_it_was_destroyed)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  CHECK(scene.execute(queue_b, other_list));
  CHECK_EQ(scene.first_queue.signalled, 1u);
  CHECK_EQ(scene.second_queue.signalled, 2u);
  CHECK(scene.session().on_Destroy12(target));
  scene.device.fences[0].completed = 1u;
  scene.reap();
  CHECK_EQ(scene.dst.references(), 2u);
  scene.device.fences[1].completed = 1u;
  scene.reap();
  CHECK_EQ(scene.dst.references(), 2u);
  scene.device.fences[1].completed = 2u;
  scene.reap();
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  scene.reap();
  CHECK_EQ(scene.dst.references(), 0u); }

TEST(d3d12_session_execution, a_queue_that_cannot_be_followed_runs_nothing_and_removes_the_device)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  scene.device.refuse_fence = true;
  CHECK(!scene.execute(queue_a, list));
  CHECK_EQ(scene.first_queue.executions, 0u);
  CHECK_EQ(scene.first_queue.signals, 0u); }

TEST(d3d12_session_execution, a_queue_whose_signal_fails_removes_the_device)
{ Scene scene;
  scene.rig.hurry(quick);
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  scene.first_queue.refuse_signal = true;
  CHECK(!scene.execute(queue_a, list));
  CHECK(scene.session().removed_reason() != 0u); }

TEST(d3d12_session_execution, an_allocator_is_not_reset_while_a_list_recorded_from_it_may_still_run)
{ Scene scene;
  scene.rig.hurry(quick);
  CHECK(scene.execute(queue_a, list));
  CHECK(!scene.session().on_ResetCommandAllocator12(allocator));
  CHECK_EQ(scene.session().removed_reason(), device_hung); }

TEST(d3d12_session_execution, an_allocator_is_reset_once_the_gpu_is_past_its_lists)
{ Scene scene;
  scene.rig.hurry(quick);
  CHECK(scene.execute(queue_a, list));
  scene.device.fences[0].completed = 1u;
  CHECK(scene.session().on_ResetCommandAllocator12(allocator));
  CHECK_EQ(scene.session().removed_reason(), 0u); }

TEST(d3d12_session_execution, an_allocator_no_list_ran_from_is_reset_at_once)
{ Scene scene;
  scene.rig.hurry(quick);
  CHECK(scene.session().on_ResetCommandAllocator12(allocator));
  CHECK_EQ(scene.session().removed_reason(), 0u); }

TEST(d3d12_session_execution, the_marks_of_the_lists_that_ran_from_an_allocator_are_the_latest_of_them)
{ Scene scene;
  scene.rig.hurry(quick);
  CHECK(scene.execute(queue_a, list));
  CHECK(scene.execute(queue_b, other_list));
  scene.device.fences[0].completed = 1u;
  CHECK(!scene.session().on_ResetCommandAllocator12(allocator)); }

TEST(d3d12_session_execution, a_session_whose_gpu_never_finishes_leaks_what_it_holds_instead_of_releasing_it)
{ Scene scene;
  scene.rig.hurry(quick);
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  scene.session().shut_down();
  CHECK_EQ(scene.dst.references(), 1u); }

TEST(d3d12_session_execution, a_drain_the_gpu_does_not_finish_gives_up_at_its_budget_and_not_at_the_hang_limit)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  av::MonotonicClock  clock;
  av::TeardownBudget  budget(clock, 20u * 1000u * 1000u);
  const std::uint64_t started = clock.now_nanoseconds();
  CHECK(!scene.session().drain_within(budget));
  CHECK(clock.now_nanoseconds() - started < 1000u * 1000u * 1000u);
  CHECK_EQ(scene.session().removed_reason(), 0u);
  scene.rig.hurry(quick);
  scene.session().shut_down(); }

TEST(d3d12_session_execution, a_drain_the_gpu_finishes_in_time_reports_it)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  for (av::FakeFence &fence : scene.device.fences) fence.completed = ~std::uint64_t{0};
  av::MonotonicClock  clock;
  av::TeardownBudget  budget(clock, 500u * 1000u * 1000u);
  CHECK(scene.session().drain_within(budget)); }

TEST(d3d12_session_execution, a_resource_a_view_names_outlives_the_guest_s_destroy_of_it)
{ Scene scene;
  ViewDesc view = {};
  view.resource = target;
  CHECK(scene.session().on_CreateUnorderedAccessView12(DescriptorRef{heap, 1u}, 0u, view));
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK(scene.session().on_Destroy12(target));
  CHECK_EQ(scene.dst.references(), 1u);
  const std::uint32_t heaps[1] = {heap};
  CHECK(scene.session().on_SetDescriptorHeaps12(list, 1u, heaps));
  CHECK(scene.session().on_SetRootDescriptorTable12(list, 1u, 0u, DescriptorRef{heap, 1u}));
  CHECK_EQ(scene.dst.references(), 1u);
  ViewDesc other = {};
  other.resource = source;
  CHECK(scene.session().on_CreateUnorderedAccessView12(DescriptorRef{heap, 1u}, 0u, other));
  CHECK_EQ(scene.dst.references(), 0u);
  CHECK_EQ(scene.src.references(), 2u); }

TEST(d3d12_session_execution, overwriting_a_slot_a_running_list_reads_waits_for_the_run)
{ Scene scene;
  ViewDesc view = {};
  view.resource = target;
  CHECK(scene.session().on_CreateUnorderedAccessView12(DescriptorRef{heap, 1u}, 0u, view));
  CHECK(scene.execute(queue_a, list));
  CHECK(scene.session().on_Destroy12(target));
  ViewDesc other = {};
  other.resource = source;
  CHECK(scene.session().on_CreateUnorderedAccessView12(DescriptorRef{heap, 1u}, 0u, other));
  CHECK_EQ(scene.dst.references(), 2u);
  scene.device.fences[0].completed = 1u;
  scene.reap();
  CHECK_EQ(scene.dst.references(), 0u); }

TEST(d3d12_session_execution, copying_descriptors_copies_what_keeps_the_resource_alive)
{ Scene scene;
  ViewDesc view = {};
  view.resource = target;
  CHECK(scene.session().on_CreateUnorderedAccessView12(DescriptorRef{heap, 1u}, 0u, view));
  CHECK(scene.session().on_CopyDescriptors12(DescriptorRef{heap, 5u}, DescriptorRef{heap, 1u}, 1u));
  CHECK_EQ(scene.device.copies, 1u);
  CHECK_EQ(scene.dst.references(), 3u);
  CHECK(scene.session().on_Destroy12(target));
  CHECK(scene.session().on_CopyDescriptors12(DescriptorRef{heap, 1u}, DescriptorRef{heap, 2u}, 1u));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(scene.session().on_CopyDescriptors12(DescriptorRef{heap, 5u}, DescriptorRef{heap, 2u}, 1u));
  CHECK_EQ(scene.dst.references(), 0u); }

TEST(d3d12_session_execution, a_destroyed_heap_keeps_its_resources_for_the_lists_that_name_it)
{ Scene scene;
  ViewDesc view = {};
  view.resource = target;
  CHECK(scene.session().on_CreateUnorderedAccessView12(DescriptorRef{heap, 1u}, 0u, view));
  const std::uint32_t heaps[1] = {heap};
  CHECK(scene.session().on_SetDescriptorHeaps12(list, 1u, heaps));
  CHECK(scene.session().on_Destroy12(heap));
  CHECK(scene.session().on_Destroy12(target));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK_EQ(scene.heap_object.references(), 1u);
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 0u);
  CHECK_EQ(scene.heap_object.references(), 0u); }

TEST(d3d12_session_execution, a_render_target_view_of_a_destroyed_resource_still_has_its_resource_when_recorded)
{ Scene scene;
  ViewDesc view = {};
  view.resource = target;
  CHECK(scene.session().on_CreateRenderTargetView12(DescriptorRef{render_heap, 0u}, 0u, view));
  CHECK(scene.session().on_Destroy12(target));
  CHECK_EQ(scene.dst.references(), 1u);
  const DescriptorRef targets[1] = {DescriptorRef{render_heap, 0u}};
  CHECK(scene.session().on_SetRenderTargets12(list, 1u, targets, 0u, DescriptorRef{}));
  CHECK_EQ(scene.dst.references(), 2u);
  ViewDesc other = {};
  other.resource = source;
  CHECK(scene.session().on_CreateRenderTargetView12(DescriptorRef{render_heap, 0u}, 0u, other));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 0u); }

TEST(d3d12_session_execution, tile_mappings_copied_between_resources_carry_the_heaps_they_map)
{ Scene scene;
  av::D3d12ObjectHolds &holds = scene.rig.holds();
  CHECK_STATUS(holds.hold(tiled, &scene.heap_object, nullptr), av::Status::ok);
  const TileCoordinate start = {0u, 0u, 0u, 0u};
  const TileSize       one = {1u, 0u, 0u, 0u, 0u};
  CHECK(scene.session().on_CopyTileMappings12(queue_a, tiled_copy, start, tiled, start, one, 0u));
  CHECK(holds.holding(tiled_copy, &scene.heap_object));
  CHECK_EQ(scene.heap_object.references(), 3u);
  CHECK(scene.session().on_Destroy12(heap));
  CHECK_EQ(scene.heap_object.references(), 3u);
  CHECK(scene.session().on_Destroy12(tiled));
  CHECK(scene.session().on_Destroy12(tiled_copy));
  scene.device.fences[0].completed = 1u;
  scene.reap();
  CHECK_EQ(scene.heap_object.references(), 0u); }

TEST(d3d12_session_execution, a_list_recorded_against_a_mapped_resource_holds_the_heap_too)
{ Scene scene;
  av::D3d12ObjectHolds &holds = scene.rig.holds();
  CHECK_STATUS(holds.hold(target, &scene.heap_object, nullptr), av::Status::ok);
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 16u));
  CHECK(holds.holding(list, &scene.heap_object));
  CHECK_EQ(scene.heap_object.references(), 3u); }

TEST(d3d12_session_execution, a_drain_that_timed_out_is_retried_against_the_same_mark_and_marks_again_only_after_new_work)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.execute(queue_a, list));
  scene.rig.hurry(quick);
  av::MonotonicClock clock;
  av::TeardownBudget first(clock, 0u);
  CHECK(!scene.session().drain_within(first));
  std::uint64_t marked = 0u, reached = 0u;
  scene.session().gpu_progress(marked, reached);
  const std::uint64_t after_first = marked;
  av::TeardownBudget second(clock, 0u);
  CHECK(!scene.session().drain_within(second));
  scene.session().gpu_progress(marked, reached);
  CHECK_EQ(marked, after_first);
  for (av::FakeFence &fence : scene.device.fences) fence.completed = ~std::uint64_t{0};
  av::TeardownBudget third(clock, 0u);
  CHECK(scene.session().drain_within(third));
  scene.session().gpu_progress(marked, reached);
  CHECK_EQ(marked, after_first);
  scene.session().shut_down(); }
