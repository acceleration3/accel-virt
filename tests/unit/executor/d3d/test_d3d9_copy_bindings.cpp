#include "core/support/status.hpp"
#include "executor/d3d/d3d9_copy_bindings.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d9CopyBindings;
using av::MockPlatform;
using av::Status;

namespace {

char objects[16];

IDirect3DDevice9   *device(std::uint32_t index) { return reinterpret_cast<IDirect3DDevice9 *>(&objects[index]); }
IDirect3DResource9 *buffer(std::uint32_t index) { return reinterpret_cast<IDirect3DResource9 *>(&objects[8u + index]); }

class Visits {
public:
    void operator()(std::uint32_t resource, IDirect3DResource9 *held)
    {
        if (count_ < 64u) {
            resources_[count_] = resource;
            buffers_[count_] = held;
        }
        count_++;
    }
    std::uint32_t count() const { return count_; }
    std::uint32_t resource(std::uint32_t index) const { return resources_[index]; }
    IDirect3DResource9 *buffer_at(std::uint32_t index) const { return buffers_[index]; }
    bool saw(std::uint32_t resource) const
    {
        for (std::uint32_t index = 0u; index < count_ && index < 64u; index++)
            if (resources_[index] == resource) return true;
        return false;
    }

private:
    std::uint32_t       count_ = 0u;
    std::uint32_t       resources_[64] = {};
    IDirect3DResource9 *buffers_[64] = {};
};

struct Dropped {
    std::uint32_t count = 0u;
    std::uint32_t resources[16] = {};
    void          operator()(std::uint32_t resource)
    {
        if (count < 16u) resources[count] = resource;
        count++;
    }
    bool saw(std::uint32_t resource) const
    {
        for (std::uint32_t index = 0u; index < count && index < 16u; index++)
            if (resources[index] == resource) return true;
        return false;
    }
};

Visits due(D3d9CopyBindings &bindings, IDirect3DDevice9 *on)
{
    Visits visits;
    bindings.due(on, [&visits](std::uint32_t resource, IDirect3DResource9 *held) { visits(resource, held); });
    return visits;
}

}

TEST(d3d9_copy_bindings, starts_with_no_copies_and_visits_nothing)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_EQ(bindings.count(), 0u);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK(bindings.invariants_hold()); }

TEST(d3d9_copy_bindings_add, remembers_each_copy_by_resource)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.add(11u, buffer(1u), device(0u)), Status::ok);
  CHECK_EQ(bindings.count(), 2u);
  CHECK(bindings.invariants_hold()); }

TEST(d3d9_copy_bindings_add, adding_a_resource_again_replaces_its_copy)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 1u);
  CHECK_STATUS(bindings.add(10u, buffer(1u), device(0u)), Status::ok);
  CHECK_EQ(bindings.count(), 1u);
  const Visits again = due(bindings, device(0u));
  CHECK_EQ(again.count(), 1u);
  CHECK(again.buffer_at(0u) == buffer(1u));
  CHECK(bindings.invariants_hold()); }

TEST(d3d9_copy_bindings_add, a_failed_allocation_adds_nothing)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::no_memory);
  CHECK_EQ(bindings.count(), 0u);
  CHECK(bindings.invariants_hold());
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok); }

TEST(d3d9_copy_bindings_due, visits_a_copy_bound_to_a_stream_once_per_draw)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 3u, 10u), Status::ok);
  const Visits first = due(bindings, device(0u));
  CHECK_EQ(first.count(), 1u);
  CHECK_EQ(first.resource(0u), 10u);
  CHECK(first.buffer_at(0u) == buffer(0u));
  const Visits second = due(bindings, device(0u));
  CHECK_EQ(second.count(), 1u);
  CHECK(bindings.invariants_hold()); }

TEST(d3d9_copy_bindings_due, a_buffer_bound_to_several_streams_and_the_indices_is_copied_once_per_draw)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.add(11u, buffer(1u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 1u, 10u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 7u, 11u), Status::ok);
  CHECK_STATUS(bindings.indices_bound(device(0u), 10u), Status::ok);
  const Visits visits = due(bindings, device(0u));
  CHECK_EQ(visits.count(), 2u);
  CHECK(visits.saw(10u));
  CHECK(visits.saw(11u)); }

TEST(d3d9_copy_bindings_due, an_index_buffer_alone_is_due)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(12u, buffer(2u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.indices_bound(device(0u), 12u), Status::ok);
  const Visits visits = due(bindings, device(0u));
  CHECK_EQ(visits.count(), 1u);
  CHECK_EQ(visits.resource(0u), 12u); }

TEST(d3d9_copy_bindings_due, a_resource_that_is_not_a_copy_or_is_zero_is_never_visited)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 99u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 1u, 0u), Status::ok);
  CHECK_STATUS(bindings.indices_bound(device(0u), 0u), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u); }

TEST(d3d9_copy_bindings_due, a_copy_belonging_to_another_device_is_not_visited_for_this_one)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(1u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK_STATUS(bindings.stream_bound(device(1u), 0u, 10u), Status::ok);
  CHECK_EQ(due(bindings, device(1u)).count(), 1u); }

TEST(d3d9_copy_bindings_due, each_device_has_its_own_bound_streams)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.add(20u, buffer(1u), device(1u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(1u), 0u, 20u), Status::ok);
  const Visits zero = due(bindings, device(0u));
  const Visits one = due(bindings, device(1u));
  CHECK_EQ(zero.count(), 1u);
  CHECK_EQ(zero.resource(0u), 10u);
  CHECK_EQ(one.count(), 1u);
  CHECK_EQ(one.resource(0u), 20u); }

TEST(d3d9_copy_bindings_due, a_device_that_never_bound_anything_visits_nothing)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK_EQ(due(bindings, device(3u)).count(), 0u); }

TEST(d3d9_copy_bindings_due, a_rebound_stream_forgets_what_it_held_before)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.add(11u, buffer(1u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 2u, 10u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 2u, 11u), Status::ok);
  const Visits visits = due(bindings, device(0u));
  CHECK_EQ(visits.count(), 1u);
  CHECK_EQ(visits.resource(0u), 11u); }

TEST(d3d9_copy_bindings_stream_bound, the_last_stream_is_kept_and_one_past_it_is_ignored)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), D3d9CopyBindings::most_streams, 10u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0xFFFFFFFFu, 10u), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK_STATUS(bindings.stream_bound(device(0u), D3d9CopyBindings::most_streams - 1u, 10u), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 1u);
  CHECK(bindings.invariants_hold()); }

TEST(d3d9_copy_bindings_stream_bound, a_failed_allocation_binds_nothing)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::no_memory);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK_STATUS(bindings.indices_bound(device(0u), 10u), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 1u); }

TEST(d3d9_copy_bindings_drop, a_dropped_copy_is_no_longer_visited_though_still_bound)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.add(11u, buffer(1u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 1u, 11u), Status::ok);
  bindings.drop(10u);
  bindings.drop(77u);
  CHECK_EQ(bindings.count(), 1u);
  const Visits visits = due(bindings, device(0u));
  CHECK_EQ(visits.count(), 1u);
  CHECK_EQ(visits.resource(0u), 11u);
  CHECK(bindings.invariants_hold()); }

TEST(d3d9_copy_bindings_forget_sources, a_device_whose_sources_were_forgotten_draws_nothing_until_it_binds_again)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::ok);
  bindings.forget_sources(device(0u));
  bindings.forget_sources(device(5u));
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK_EQ(bindings.count(), 1u);
  CHECK_STATUS(bindings.stream_bound(device(0u), 4u, 10u), Status::ok);
  CHECK_EQ(due(bindings, device(0u)).count(), 1u); }

TEST(d3d9_copy_bindings_drop_device, visits_and_removes_that_devices_copies_and_sources_only)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  CHECK_STATUS(bindings.add(10u, buffer(0u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.add(11u, buffer(1u), device(0u)), Status::ok);
  CHECK_STATUS(bindings.add(20u, buffer(2u), device(1u)), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(0u), 0u, 10u), Status::ok);
  CHECK_STATUS(bindings.stream_bound(device(1u), 0u, 20u), Status::ok);
  Dropped dropped;
  bindings.drop_device(device(0u), [&dropped](std::uint32_t resource) { dropped(resource); });
  CHECK_EQ(dropped.count, 2u);
  CHECK(dropped.saw(10u));
  CHECK(dropped.saw(11u));
  CHECK_EQ(bindings.count(), 1u);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK_EQ(due(bindings, device(1u)).count(), 1u);
  CHECK(bindings.invariants_hold());
  Dropped none;
  bindings.drop_device(device(9u), [&none](std::uint32_t resource) { none(resource); });
  CHECK_EQ(none.count, 0u);
  CHECK_EQ(bindings.count(), 1u); }

TEST(d3d9_copy_bindings_drop_all, visits_every_copy_and_forgets_every_device)
{ MockPlatform     platform;
  D3d9CopyBindings bindings(platform);
  for (std::uint32_t resource = 1u; resource <= 5u; resource++) {
      CHECK_STATUS(bindings.add(resource, buffer(resource), device(resource % 2u)), Status::ok);
      CHECK_STATUS(bindings.stream_bound(device(resource % 2u), resource, resource), Status::ok);
  }
  Dropped dropped;
  bindings.drop_all([&dropped](std::uint32_t resource) { dropped(resource); });
  CHECK_EQ(dropped.count, 5u);
  CHECK_EQ(bindings.count(), 0u);
  CHECK_EQ(due(bindings, device(0u)).count(), 0u);
  CHECK_EQ(due(bindings, device(1u)).count(), 0u);
  CHECK(bindings.invariants_hold());
  Dropped again;
  bindings.drop_all([&again](std::uint32_t resource) { again(resource); });
  CHECK_EQ(again.count, 0u); }

TEST(d3d9_copy_bindings_invariants, a_copy_with_no_buffer_or_no_device_is_noticed)
{ MockPlatform     platform;
  D3d9CopyBindings missing_buffer(platform);
  CHECK_STATUS(missing_buffer.add(10u, nullptr, device(0u)), Status::ok);
  CHECK(!missing_buffer.invariants_hold());
  D3d9CopyBindings missing_device(platform);
  CHECK_STATUS(missing_device.add(10u, buffer(0u), nullptr), Status::ok);
  CHECK(!missing_device.invariants_hold()); }
