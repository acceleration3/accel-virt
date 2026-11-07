#include "harness/executor/d3d12_session_loopback.hpp"
#include "harness/executor/fake_allocator.hpp"
#include "harness/executor/fake_command_list.hpp"
#include "harness/executor/fake_device.hpp"
#include "harness/executor/fake_resource.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

#include <cstdint>

using av::D3dObjectKind;
using namespace av::d3d12proto;

namespace {

constexpr std::uint32_t list = 1u;
constexpr std::uint32_t resolved = 2u;
constexpr std::uint32_t multisampled = 3u;
constexpr std::uint32_t buffer = 4u;
constexpr std::uint32_t format_rgba = 28u;
constexpr std::uint32_t heap_default = 1u;
constexpr std::uint32_t mode_decompress = 0u;
constexpr std::uint32_t mode_average = 3u;
constexpr std::uint32_t mode_sampler_feedback = 4u;
constexpr std::uint32_t dimension_buffer = 1u;
constexpr std::uint32_t layout_row_major = 1u;
constexpr std::uint64_t buffer_bytes = 1024u;
constexpr std::uint64_t guest_address = 0x300000000ull;
constexpr std::uint64_t host_address = 0x7000u;

D3D12_RESOURCE_DESC texture_desc(std::uint64_t width, UINT height, UINT samples)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = samples;
    return desc;
}

std::uint32_t references_of(IUnknown *object)
{
    object->AddRef();
    return object->Release();
}

ResourceDesc buffer_of(std::uint64_t bytes)
{
    ResourceDesc desc = {};
    desc.width = bytes;
    desc.dimension = dimension_buffer;
    desc.height = 1u;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.sample_count = 1u;
    desc.layout = layout_row_major;
    return desc;
}

class Scene {
public:
    Scene()
    {
        rig.place(device);
        target.desc = texture_desc(16u, 16u, 1u);
        source.desc = texture_desc(16u, 16u, 4u);
        written.gpu_address = host_address;
        device.committed = &written;
        insert(list, D3dObjectKind::list12, &commands);
        insert(resolved, D3dObjectKind::resource12, &target);
        insert(multisampled, D3dObjectKind::resource12, &source);
        std::uint32_t answer = 1u;
        CHECK(av_d3d12_CreateCommittedResource12(rig, buffer, HeapProperties{heap_default, 0u, 0u, 0u}, 0u,
                                                 buffer_of(buffer_bytes), 0u, ClearValue{}, guest_address, &answer));
        CHECK_EQ(answer, 0u);
    }

    void insert(std::uint32_t id, D3dObjectKind kind, IUnknown *object)
    {
        CHECK_STATUS(rig.objects().insert(id, kind, object), av::Status::ok);
    }

    av::D3d12Session &session() { return rig.session(); }

    av::FakeDevice          device;
    av::FakeCommandList     commands;
    av::FakeResource        target, source, written, odd;
    av::D3d12SessionLoopback rig;
};

}

TEST(d3d12_session_extended_lists, sample_positions_are_set_for_each_pixel_of_a_grid_and_reset_by_zero)
{ Scene scene;
  SamplePosition positions[4] = {{-8, 7}, {1, 2}, {3, 4}, {-1, -2}};
  CHECK(scene.session().on_SetSamplePositions12(list, 4u, 1u, 4u, positions));
  CHECK_EQ(scene.commands.sample_positions, 1u);
  CHECK_EQ(scene.commands.sample_samples, 4u);
  CHECK_EQ(scene.commands.sample_pixels, 1u);
  CHECK_EQ(scene.commands.sample_first_x, -8);
  SamplePosition grid[16] = {};
  CHECK(scene.session().on_SetSamplePositions12(list, 4u, 4u, 16u, grid));
  CHECK_EQ(scene.commands.sample_positions, 2u);
  CHECK(scene.session().on_SetSamplePositions12(list, 0u, 0u, 0u, nullptr));
  CHECK_EQ(scene.commands.sample_positions, 3u);
  CHECK_EQ(scene.commands.sample_samples, 0u);
  CHECK(scene.session().invariants_hold()); }

TEST(d3d12_session_extended_lists, sample_positions_that_do_not_fit_a_grid_or_the_pixel_are_refused)
{ Scene scene;
  SamplePosition positions[16] = {};
  CHECK(!scene.session().on_SetSamplePositions12(list, 3u, 1u, 3u, positions));
  CHECK(!scene.session().on_SetSamplePositions12(list, 4u, 2u, 8u, positions));
  CHECK(!scene.session().on_SetSamplePositions12(list, 4u, 1u, 3u, positions));
  CHECK(!scene.session().on_SetSamplePositions12(list, 32u, 4u, 128u, positions));
  CHECK(!scene.session().on_SetSamplePositions12(list, 0u, 1u, 0u, positions));
  positions[2].y = 8;
  CHECK(!scene.session().on_SetSamplePositions12(list, 4u, 1u, 4u, positions));
  positions[2].y = -9;
  CHECK(!scene.session().on_SetSamplePositions12(list, 4u, 1u, 4u, positions));
  positions[2].y = 0;
  positions[3].x = 8;
  CHECK(!scene.session().on_SetSamplePositions12(list, 4u, 1u, 4u, positions));
  positions[3].x = -9;
  CHECK(!scene.session().on_SetSamplePositions12(list, 4u, 1u, 4u, positions));
  positions[3].x = 7;
  CHECK(scene.session().on_SetSamplePositions12(list, 4u, 1u, 4u, positions));
  CHECK_EQ(scene.commands.sample_positions, 1u); }

TEST(d3d12_session_extended_lists, a_list_without_the_extended_interface_refuses_what_needs_it)
{ Scene scene;
  scene.commands.extended = false;
  SamplePosition positions[1] = {};
  ImmediateWrite write = {guest_address, 1u, 0u};
  CHECK(!scene.session().on_SetSamplePositions12(list, 1u, 1u, 1u, positions));
  CHECK(!scene.session().on_SetViewInstanceMask12(list, 3u));
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_average));
  CHECK_EQ(scene.commands.recorded, 0u);
  CHECK_EQ(references_of(&scene.commands), 1u); }

TEST(d3d12_session_extended_lists, the_view_instance_mask_reaches_the_list)
{ Scene scene;
  CHECK(scene.session().on_SetViewInstanceMask12(list, 0x5u));
  CHECK_EQ(scene.commands.view_instance_mask, 0x5u);
  CHECK(scene.session().on_SetViewInstanceMask12(list, 0u));
  CHECK_EQ(scene.commands.view_instance_mask, 0u);
  CHECK(!scene.session().on_SetViewInstanceMask12(list + 100u, 1u));
  CHECK_EQ(references_of(&scene.commands), 1u); }

TEST(d3d12_session_extended_lists, a_region_resolve_lands_the_source_rect_at_the_destination_offset)
{ Scene scene;
  CHECK(scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_average));
  CHECK(!scene.commands.resolve_rect_given);
  CHECK(scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 4u, 4u, multisampled, 0u, 1u, Rect{0, 0, 12, 12}, format_rgba, mode_average));
  CHECK_EQ(scene.commands.region_resolves, 2u);
  CHECK_EQ(scene.commands.resolve_x, 4u);
  CHECK_EQ(scene.commands.resolve_y, 4u);
  CHECK(scene.commands.resolve_mode == D3D12_RESOLVE_MODE_AVERAGE);
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 8u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_average));
  CHECK_EQ(scene.commands.region_resolves, 2u);
  CHECK(scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 8u, 8u, multisampled, 0u, 1u, Rect{0, 0, 8, 8}, format_rgba, mode_average));
  CHECK(scene.commands.resolve_rect_given);
  CHECK_EQ(scene.commands.region_resolves, 3u);
  CHECK(scene.session().invariants_hold()); }

TEST(d3d12_session_extended_lists, a_region_resolve_that_leaves_either_resource_or_names_no_area_is_refused)
{ Scene scene;
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 9u, 8u, multisampled, 0u, 1u, Rect{0, 0, 8, 8}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 8u, 9u, multisampled, 0u, 1u, Rect{0, 0, 8, 8}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 1u, Rect{0, 0, 17, 8}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 1u, Rect{0, 0, 8, 17}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 1u, Rect{4, 0, 4, 8}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 1u, Rect{0, 4, 8, 4}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 1u, Rect{-1, 0, 8, 8}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0xFFFFFFFFu, 0u, multisampled, 0u, 1u, Rect{0, 0, 8, 8}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 1u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 1u, 0u, Rect{}, format_rgba, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, 61u, mode_average));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_sampler_feedback));
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_decompress));
  CHECK_EQ(scene.commands.region_resolves, 0u);
  CHECK_EQ(references_of(&scene.target), 1u); }

TEST(d3d12_session_extended_lists, a_region_resolve_needs_a_multisampled_source_unless_it_decompresses)
{ Scene scene;
  scene.source.desc = texture_desc(16u, 16u, 1u);
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_average));
  CHECK(scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_decompress));
  scene.target.desc = texture_desc(16u, 16u, 4u);
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_decompress));
  scene.source.desc = texture_desc(16u, 16u, 4u);
  CHECK(!scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_average));
  CHECK(scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_decompress));
  CHECK_EQ(scene.commands.region_resolves, 2u); }

TEST(d3d12_session_extended_lists, a_region_resolve_keeps_both_resources_alive_until_the_list_is_destroyed)
{ Scene scene;
  CHECK(scene.session().on_ResolveSubresourceRegion12(list, resolved, 0u, 0u, 0u, multisampled, 0u, 0u, Rect{}, format_rgba, mode_average));
  CHECK_EQ(references_of(&scene.target), 2u);
  CHECK_EQ(references_of(&scene.source), 2u);
  CHECK(scene.session().on_Destroy12(resolved));
  CHECK(scene.session().on_Destroy12(multisampled));
  CHECK_EQ(references_of(&scene.target), 1u);
  CHECK_EQ(references_of(&scene.source), 1u);
  CHECK(scene.session().on_Destroy12(list));
  CHECK_EQ(references_of(&scene.target), 0u);
  CHECK_EQ(references_of(&scene.source), 0u); }

TEST(d3d12_session_extended_lists, immediate_writes_go_to_the_host_address_of_a_buffer_in_order)
{ Scene scene;
  const ImmediateWrite writes[3] = {{guest_address, 1u, 0u}, {guest_address + 8u, 2u, 1u}, {guest_address + buffer_bytes - 4u, 3u, 2u}};
  CHECK(scene.session().on_WriteBufferImmediate12(list, 3u, writes));
  CHECK_EQ(scene.commands.immediate_writes, 3u);
  CHECK_EQ(scene.commands.immediate_last_address, host_address + buffer_bytes - 4u);
  CHECK(scene.commands.immediate_last_mode == D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT);
  CHECK(scene.session().invariants_hold()); }

TEST(d3d12_session_extended_lists, immediate_writes_outside_a_buffer_or_misaligned_or_too_many_are_refused)
{ Scene scene;
  ImmediateWrite write = {guest_address + buffer_bytes - 3u, 1u, 0u};
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  write.address = guest_address + buffer_bytes;
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  write.address = guest_address + 2u;
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  write.address = 0u;
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  write.address = guest_address;
  write.mode = 3u;
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  write.mode = 0u;
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 0u, &write));
  ImmediateWrite many[65] = {};
  for (ImmediateWrite &each : many) each.address = guest_address;
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 65u, many));
  CHECK(scene.session().on_WriteBufferImmediate12(list, 64u, many));
  CHECK_EQ(scene.commands.immediate_writes, 64u); }

TEST(d3d12_session_extended_lists, an_immediate_write_must_fit_its_four_bytes_inside_the_buffer)
{ Scene scene;
  scene.odd.gpu_address = 0x9000u;
  scene.device.committed = &scene.odd;
  constexpr std::uint64_t odd_address = 0x400000000ull;
  std::uint32_t answer = 1u;
  CHECK(av_d3d12_CreateCommittedResource12(scene.rig, 9u, HeapProperties{heap_default, 0u, 0u, 0u}, 0u,
                                           buffer_of(1026u), 0u, ClearValue{}, odd_address, &answer));
  CHECK_EQ(answer, 0u);
  ImmediateWrite write = {odd_address + 1020u, 1u, 0u};
  CHECK(scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  write.address = odd_address + 1024u;
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  CHECK_EQ(scene.commands.immediate_writes, 1u); }

TEST(d3d12_session_extended_lists, a_buffer_written_immediately_stays_alive_after_the_guest_destroys_it)
{ Scene scene;
  const ImmediateWrite write = {guest_address, 7u, 0u};
  CHECK(scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  CHECK_EQ(references_of(&scene.written), 3u);
  CHECK(scene.session().on_Destroy12(buffer));
  CHECK_EQ(references_of(&scene.written), 2u);
  CHECK(!scene.session().on_WriteBufferImmediate12(list, 1u, &write));
  CHECK(scene.session().on_Destroy12(list));
  CHECK_EQ(references_of(&scene.written), 1u); }
