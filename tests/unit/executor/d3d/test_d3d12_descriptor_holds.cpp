#include "executor/d3d/d3d12_descriptor_holds.hpp"
#include "executor/d3d/d3d12_object_holds.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::CountedObject;
using av::D3d12DescriptorHolds;
using av::D3d12ObjectHolds;

namespace {

constexpr std::uint32_t heap_a = 5u;
constexpr std::uint32_t heap_b = 6u;
constexpr std::uint32_t list_a = 11u;

struct Rig {
    av::MockPlatform platform;
    D3d12ObjectHolds objects{platform};
    D3d12DescriptorHolds descriptors{platform, objects};
    std::uint64_t        reached = 0u;

    av::Status note(std::uint32_t heap, std::uint32_t index, IUnknown *resource, IUnknown *counter,
                    std::uint64_t marked)
    {
        return descriptors.note(heap, index, resource, counter, marked, [this]() { return reached; });
    }
};

}

TEST(d3d12_descriptor_holds, a_view_keeps_its_resource_and_its_counter_alive)
{ Rig rig;
  CountedObject resource, counter;
  CHECK_STATUS(rig.note(heap_a, 3u, &resource, &counter, 0u), av::Status::ok);
  CHECK_EQ(resource.references(), 2u);
  CHECK_EQ(counter.references(), 2u);
  CHECK(rig.descriptors.holds(heap_a, 3u, &resource));
  CHECK(rig.descriptors.holds(heap_a, 3u, &counter));
  CHECK(!rig.descriptors.holds(heap_a, 4u, &resource));
  CHECK(!rig.descriptors.holds(heap_b, 3u, &resource));
  CHECK_EQ(rig.descriptors.count(), 1u);
  CHECK(rig.descriptors.invariants_hold());
  rig.descriptors.release_all();
  CHECK_EQ(resource.references(), 1u);
  CHECK_EQ(counter.references(), 1u);
  CHECK_EQ(rig.descriptors.count(), 0u); }

TEST(d3d12_descriptor_holds, overwriting_a_slot_gives_the_old_resource_up_only_when_the_gpu_is_past_the_mark)
{ Rig rig;
  CountedObject old_resource, new_resource;
  CHECK_STATUS(rig.note(heap_a, 0u, &old_resource, nullptr, 0u), av::Status::ok);
  rig.reached = 3u;
  CHECK_STATUS(rig.note(heap_a, 0u, &new_resource, nullptr, 7u), av::Status::ok);
  CHECK_EQ(old_resource.references(), 2u);
  CHECK_EQ(new_resource.references(), 2u);
  CHECK_EQ(rig.objects.retired(), 1u);
  rig.objects.reap(6u);
  CHECK_EQ(old_resource.references(), 2u);
  rig.objects.reap(7u);
  CHECK_EQ(old_resource.references(), 1u);
  CHECK(rig.descriptors.holds(heap_a, 0u, &new_resource)); }

TEST(d3d12_descriptor_holds, overwriting_a_slot_when_the_gpu_is_idle_releases_at_once)
{ Rig rig;
  CountedObject old_resource, new_resource;
  CHECK_STATUS(rig.note(heap_a, 0u, &old_resource, nullptr, 4u), av::Status::ok);
  rig.reached = 4u;
  CHECK_STATUS(rig.note(heap_a, 0u, &new_resource, nullptr, 4u), av::Status::ok);
  CHECK_EQ(old_resource.references(), 1u);
  CHECK_EQ(rig.objects.retired(), 0u); }

TEST(d3d12_descriptor_holds, a_view_of_nothing_clears_the_slot)
{ Rig rig;
  CountedObject resource;
  CHECK_STATUS(rig.note(heap_a, 0u, &resource, nullptr, 0u), av::Status::ok);
  CHECK_STATUS(rig.note(heap_a, 0u, nullptr, nullptr, 0u), av::Status::ok);
  CHECK_EQ(resource.references(), 1u);
  CHECK_EQ(rig.descriptors.count(), 0u);
  CHECK_STATUS(rig.note(heap_a, 9u, nullptr, nullptr, 0u), av::Status::ok);
  CHECK(rig.descriptors.invariants_hold()); }

TEST(d3d12_descriptor_holds, copying_descriptors_copies_the_references_and_clears_slots_copied_from_nothing)
{ Rig rig;
  CountedObject first, second, stale;
  CHECK_STATUS(rig.note(heap_a, 0u, &first, nullptr, 0u), av::Status::ok);
  CHECK_STATUS(rig.note(heap_a, 2u, &second, nullptr, 0u), av::Status::ok);
  CHECK_STATUS(rig.note(heap_b, 11u, &stale, nullptr, 0u), av::Status::ok);
  CHECK_STATUS(rig.descriptors.copy(heap_b, 10u, heap_a, 0u, 3u, 0u, [&rig]() { return rig.reached; }), av::Status::ok);
  CHECK(rig.descriptors.holds(heap_b, 10u, &first));
  CHECK(!rig.descriptors.holds(heap_b, 11u, &stale));
  CHECK(rig.descriptors.holds(heap_b, 12u, &second));
  CHECK_EQ(first.references(), 3u);
  CHECK_EQ(second.references(), 3u);
  CHECK_EQ(stale.references(), 1u);
  CHECK(rig.descriptors.holds(heap_a, 0u, &first));
  CHECK(rig.descriptors.invariants_hold()); }

TEST(d3d12_descriptor_holds, a_destroyed_heap_gives_its_references_up_after_the_gpu_and_keeps_them_for_lists_holding_it)
{ Rig rig;
  CountedObject heap_object, held_by_list, other_heap_resource;
  CountedObject idle;
  CHECK_STATUS(rig.note(heap_a, 0u, &held_by_list, nullptr, 0u), av::Status::ok);
  CHECK_STATUS(rig.note(heap_b, 0u, &other_heap_resource, nullptr, 0u), av::Status::ok);
  CHECK_STATUS(rig.objects.hold(list_a, &heap_object, nullptr), av::Status::ok);
  rig.objects.submitted(list_a, 8u);
  rig.reached = 2u;
  CHECK_STATUS(rig.descriptors.destroy_heap(heap_a, &heap_object, 8u, [&rig]() { return rig.reached; }),
               av::Status::ok);
  CHECK_EQ(rig.descriptors.count(), 1u);
  CHECK(rig.objects.holding(list_a, &held_by_list));
  CHECK_EQ(held_by_list.references(), 3u);
  rig.objects.reap(8u);
  CHECK_EQ(held_by_list.references(), 2u);
  CHECK_STATUS(rig.objects.release_list(list_a, 2u), av::Status::ok);
  rig.objects.reap(8u);
  CHECK_EQ(held_by_list.references(), 1u);
  CHECK_EQ(other_heap_resource.references(), 2u);
  CHECK(rig.objects.invariants_hold()); }

TEST(d3d12_descriptor_holds, a_destroyed_heap_no_list_holds_releases_its_references_with_the_gpu)
{ Rig rig;
  CountedObject heap_object, resource;
  CHECK_STATUS(rig.note(heap_a, 0u, &resource, nullptr, 0u), av::Status::ok);
  CHECK_STATUS(rig.descriptors.destroy_heap(heap_a, &heap_object, 0u, [&rig]() { return rig.reached; }),
               av::Status::ok);
  CHECK_EQ(resource.references(), 1u);
  CHECK_EQ(rig.descriptors.count(), 0u); }

TEST(d3d12_descriptor_holds, a_slot_that_cannot_be_kept_takes_no_reference)
{ Rig rig;
  CountedObject resource;
  rig.platform.fail_allocation_at(1u);
  CHECK_STATUS(rig.note(heap_a, 0u, &resource, nullptr, 0u), av::Status::no_memory);
  CHECK_EQ(resource.references(), 1u);
  CHECK_EQ(rig.descriptors.count(), 0u);
  CHECK(rig.descriptors.invariants_hold()); }

TEST(d3d12_object_holds, a_list_that_holds_a_holder_inherits_what_the_holder_names)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject holder, extra, other_list_object;
  CHECK_STATUS(holds.hold(list_a, &holder, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(12u, &other_list_object, nullptr), av::Status::ok);
  CHECK_STATUS(holds.inherit(&holder, &extra), av::Status::ok);
  CHECK(holds.holding(list_a, &extra));
  CHECK(!holds.holding(12u, &extra));
  CHECK_EQ(extra.references(), 2u);
  CHECK_STATUS(holds.inherit(&holder, &extra), av::Status::ok);
  CHECK_EQ(extra.references(), 2u);
  CHECK_STATUS(holds.inherit(&holder, nullptr), av::Status::invalid_parameter);
  holds.release_all(); }

TEST(d3d12_descriptor_holds, the_resource_a_slot_names_can_be_asked_for)
{ Rig rig;
  CountedObject resource, counter;
  CHECK(rig.descriptors.resource_at(heap_a, 2u) == nullptr);
  CHECK_STATUS(rig.note(heap_a, 2u, &resource, &counter, 0u), av::Status::ok);
  CHECK(rig.descriptors.resource_at(heap_a, 2u) == &resource);
  CHECK(rig.descriptors.resource_at(heap_a, 3u) == nullptr);
  CHECK(rig.descriptors.resource_at(heap_b, 2u) == nullptr);
  rig.descriptors.release_all(); }
