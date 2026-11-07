#include "executor/d3d/d3d12_indirect_stride.hpp"
#include "harness/executor/d3d12_answers.hpp"
#include "harness/executor/d3d12_session_loopback.hpp"
#include "harness/executor/fake_allocator.hpp"
#include "harness/executor/fake_command_list.hpp"
#include "harness/executor/fake_descriptor_heap.hpp"
#include "harness/executor/fake_pipeline.hpp"
#include "harness/executor/fake_resource.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

#include <cstdint>

using av::D3dObjectKind;
using namespace av::d3d12proto;

namespace {

constexpr std::uint32_t list = 1u;
constexpr std::uint32_t allocator = 2u;
constexpr std::uint32_t target = 3u;
constexpr std::uint32_t source = 4u;
constexpr std::uint32_t texture = 5u;
constexpr std::uint32_t other_texture = 6u;
constexpr std::uint32_t pipeline = 7u;
constexpr std::uint32_t root = 8u;
constexpr std::uint32_t descriptors = 9u;
constexpr std::uint32_t queries = 10u;
constexpr std::uint32_t signature = 11u;
constexpr std::uint32_t arguments = 12u;
constexpr std::uint32_t counts = 13u;
constexpr std::uint32_t stride = 16u;
constexpr std::uint32_t format_rgba = 28u;
constexpr std::uint32_t format_r8 = 61u;
constexpr std::uint32_t format_r32 = 41u;
constexpr std::uint32_t format_rgba_uint = 30u;
constexpr std::uint32_t location_subresource = 0u;
constexpr std::uint32_t location_footprint = 1u;
constexpr std::uint32_t driver_internal_error = 0x887A0020u;
constexpr std::uint32_t out_of_memory = 0x8007000Eu;
constexpr std::uint64_t buffer_bytes = 1024u;
constexpr std::uint64_t most_heap_bytes = std::uint64_t{1} << 34u;

D3D12_RESOURCE_DESC buffer_desc(std::uint64_t bytes)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1u;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.SampleDesc.Count = 1u;
    return desc;
}

D3D12_RESOURCE_DESC texture_desc(DXGI_FORMAT format, std::uint64_t width, UINT height, UINT samples = 1u)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.Format = format;
    desc.SampleDesc.Count = samples;
    return desc;
}

std::uint32_t references_of(IUnknown *object)
{
    object->AddRef();
    return object->Release();
}

CopyLocation subresource_of(std::uint32_t resource)
{
    CopyLocation made = {};
    made.resource = resource;
    made.type = location_subresource;
    return made;
}

CopyLocation footprint_of(std::uint32_t resource, std::uint32_t format, std::uint32_t width, std::uint32_t height)
{
    CopyLocation made = {};
    made.resource = resource;
    made.type = location_footprint;
    made.format = format;
    made.width = width;
    made.height = height;
    made.depth = 1u;
    made.row_pitch = 256u;
    return made;
}

class Scene {
public:
    Scene()
    {
        dst.desc = buffer_desc(buffer_bytes);
        src.desc = buffer_desc(buffer_bytes);
        argument_buffer.desc = buffer_desc(buffer_bytes);
        count_buffer.desc = buffer_desc(buffer_bytes);
        rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u);
        other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u);
        heap.desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap.desc.NumDescriptors = 8u;
        heap.desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        CHECK(av::D3d12IndirectStride(stride).stamp(&command_signature));
        insert(list, D3dObjectKind::list12, &commands);
        insert(allocator, D3dObjectKind::allocator12, &alloc);
        insert(target, D3dObjectKind::resource12, &dst);
        insert(source, D3dObjectKind::resource12, &src);
        insert(texture, D3dObjectKind::resource12, &rgba);
        insert(other_texture, D3dObjectKind::resource12, &other_rgba);
        insert(pipeline, D3dObjectKind::pipeline12, &state);
        insert(root, D3dObjectKind::root_signature12, &root_signature);
        insert(descriptors, D3dObjectKind::descriptor_heap12, &heap);
        insert(queries, D3dObjectKind::query_heap12, &query_heap);
        insert(signature, D3dObjectKind::command_signature12, &command_signature);
        insert(arguments, D3dObjectKind::resource12, &argument_buffer);
        insert(counts, D3dObjectKind::resource12, &count_buffer);
        CHECK_STATUS(rig.queries().remember(queries, 0u, 16u), av::Status::ok);
    }

    void insert(std::uint32_t id, D3dObjectKind kind, IUnknown *object)
    {
        CHECK_STATUS(rig.objects().insert(id, kind, object), av::Status::ok);
    }

    av::D3d12Session &session() { return rig.session(); }

    av::FakeCommandList                      commands;
    av::FakeAllocator                        alloc;
    av::FakeResource                         dst, src, rgba, other_rgba, argument_buffer, count_buffer;
    av::FakePipeline                         state;
    av::FakeDescriptorHeap                   heap;
    av::CountedChild<ID3D12RootSignature>    root_signature;
    av::CountedChild<ID3D12QueryHeap>        query_heap;
    av::CountedChild<ID3D12CommandSignature> command_signature;
    av::D3d12SessionLoopback                 rig;
};

}

TEST(d3d12_session_isolation, a_buffer_destroyed_after_a_list_copied_into_it_stays_alive_until_the_list_is_reset)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK_EQ(scene.commands.buffer_copies, 1u);
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK_EQ(scene.src.references(), 2u);
  CHECK(scene.session().on_Destroy12(target));
  CHECK(scene.session().on_Destroy12(source));
  CHECK(scene.rig.objects().find(target, D3dObjectKind::resource12) == nullptr);
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK_EQ(scene.src.references(), 1u);
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 0u);
  CHECK_EQ(scene.src.references(), 0u);
  }

TEST(d3d12_session_isolation, a_buffer_destroyed_after_a_list_copied_into_it_stays_alive_until_the_list_is_destroyed)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.session().on_Destroy12(target));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(scene.session().on_Destroy12(list));
  CHECK_EQ(scene.dst.references(), 0u);
  CHECK_EQ(scene.src.references(), 1u);
  }

TEST(d3d12_session_isolation, a_destroyed_buffer_can_no_longer_be_recorded_against)
{ Scene scene;
  CHECK(scene.session().on_Destroy12(target));
  CHECK(!scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK_EQ(scene.commands.buffer_copies, 0u); }

TEST(d3d12_session_isolation, an_object_never_recorded_is_released_when_destroyed)
{ Scene scene;
  CHECK(scene.session().on_Destroy12(target));
  CHECK_EQ(scene.dst.references(), 0u); }

TEST(d3d12_session_isolation, a_list_that_reset_twice_holds_only_what_it_recorded_since)
{ Scene scene;
  CHECK(scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 64u));
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(scene.session().on_CopyBufferRegion12(list, source, 0u, target, 0u, 64u));
  CHECK_EQ(scene.dst.references(), 2u);
  CHECK(scene.session().on_ResetCommandList12(list, allocator, 0u));
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK_EQ(scene.src.references(), 1u); }

namespace {

using Recording = bool (*)(Scene &scene);

struct Case {
    const char *what;
    Recording   record;
    std::uint32_t kept;
};

const Case cases[] = {
    {"ResourceBarrier12",
     [](Scene &scene) {
         Barrier barrier = {};
         barrier.resource = target;
         return scene.session().on_ResourceBarrier12(list, 1u, &barrier);
     }, 1u},
    {"SetPipelineState12", [](Scene &scene) { return scene.session().on_SetPipelineState12(list, pipeline); }, 1u},
    {"SetRootSignature12", [](Scene &scene) { return scene.session().on_SetRootSignature12(list, 1u, root); }, 1u},
    {"SetDescriptorHeaps12",
     [](Scene &scene) {
         const std::uint32_t heaps[1] = {descriptors};
         return scene.session().on_SetDescriptorHeaps12(list, 1u, heaps);
     }, 1u},
    {"CopyBufferRegion12",
     [](Scene &scene) { return scene.session().on_CopyBufferRegion12(list, target, 0u, source, 0u, 16u); }, 2u},
    {"CopyResource12", [](Scene &scene) { return scene.session().on_CopyResource12(list, target, source); }, 2u},
    {"CopyTextureRegion12",
     [](Scene &scene) {
         return scene.session().on_CopyTextureRegion12(list, subresource_of(texture), 0u, 0u, 0u,
                                                       subresource_of(other_texture), 0u, Box{});
     }, 2u},
    {"ResolveSubresource12",
     [](Scene &scene) {
         scene.other_rgba.desc.SampleDesc.Count = 4u;
         return scene.session().on_ResolveSubresource12(list, texture, 0u, other_texture, 0u, format_rgba);
     }, 2u},
    {"BeginQuery12", [](Scene &scene) { return scene.session().on_BeginQuery12(list, queries, 0u, 0u); }, 1u},
    {"EndQuery12", [](Scene &scene) { return scene.session().on_EndQuery12(list, queries, 0u, 0u); }, 1u},
    {"ResolveQueryData12",
     [](Scene &scene) { return scene.session().on_ResolveQueryData12(list, queries, 0u, 0u, 1u, target, 0u); }, 2u},
    {"ExecuteIndirect12",
     [](Scene &scene) { return scene.session().on_ExecuteIndirect12(list, signature, 4u, arguments, 0u, counts, 0u); }, 3u},
};

}

TEST(d3d12_session_isolation, every_recording_that_names_an_object_keeps_it_alive_after_the_guest_destroys_it)
{ for (const Case &each : cases) {
      Scene scene;
      CHECK(each.record(scene));
      IUnknown *const named[] = {&scene.root_signature, &scene.query_heap, &scene.command_signature, &scene.state,
                                 &scene.heap,           &scene.dst,        &scene.src,               &scene.rgba,
                                 &scene.other_rgba,     &scene.argument_buffer, &scene.count_buffer};
      std::uint32_t held = 0u;
      for (IUnknown *const object : named) held += references_of(object) - 1u;
      CHECK_EQ(held, each.kept);
      for (std::uint32_t id = target; id <= counts; id++) CHECK(scene.session().on_Destroy12(id));
      std::uint32_t alive = 0u;
      for (IUnknown *const object : named) alive += references_of(object);
      CHECK_EQ(alive, each.kept);
      CHECK(scene.session().on_Destroy12(list));
      for (IUnknown *const object : named) CHECK_EQ(references_of(object), 0u);
  } }

TEST(d3d12_session_isolation, copy_resource_between_resources_that_differ_is_refused_and_removes_the_device)
{ Scene scene;
  scene.src.desc = buffer_desc(buffer_bytes / 2u);
  CHECK(!scene.session().on_CopyResource12(list, target, source));
  CHECK_EQ(scene.commands.whole_copies, 0u);
  CHECK_EQ(scene.dst.references(), 1u);
  CHECK(av_d3d12_CopyResource12(scene.rig, list, target, source));
  std::uint32_t removed = 0u;
  CHECK(av_d3d12_Finish12(scene.rig, &removed));
  CHECK_EQ(removed, driver_internal_error);
  CHECK_EQ(scene.commands.whole_copies, 0u); }

TEST(d3d12_session_isolation, copy_resource_refuses_each_way_two_resources_can_differ)
{ Scene scene;
  CHECK(!scene.session().on_CopyResource12(list, target, target));
  CHECK(!scene.session().on_CopyResource12(list, target, texture));
  CHECK(!scene.session().on_CopyResource12(list, texture, target));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 32u, 16u);
  CHECK(!scene.session().on_CopyResource12(list, texture, other_texture));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 32u);
  CHECK(!scene.session().on_CopyResource12(list, texture, other_texture));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R16_UNORM, 16u, 16u);
  CHECK(!scene.session().on_CopyResource12(list, texture, other_texture));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u, 4u);
  CHECK(!scene.session().on_CopyResource12(list, texture, other_texture));
  CHECK_EQ(scene.commands.whole_copies, 0u);
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_B8G8R8A8_UNORM, 16u, 16u);
  CHECK(!scene.session().on_CopyResource12(list, texture, other_texture));
  CHECK_EQ(scene.commands.whole_copies, 0u);
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UINT, 16u, 16u);
  CHECK(scene.session().on_CopyResource12(list, texture, other_texture));
  CHECK_EQ(scene.commands.whole_copies, 1u); }

TEST(d3d12_session_isolation, a_texture_copy_that_leaves_its_destination_mip_is_refused)
{ Scene scene;
  const CopyLocation to = subresource_of(texture);
  const CopyLocation from = subresource_of(other_texture);
  CHECK(scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 0u, from, 0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 1u);
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 1u, 0u, 0u, from, 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 1u, 0u, from, 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 1u, from, 0u, Box{}));
  const Box small = {0u, 0u, 0u, 8u, 8u, 1u};
  CHECK(scene.session().on_CopyTextureRegion12(list, to, 8u, 8u, 0u, from, 1u, small));
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 9u, 8u, 0u, from, 1u, small));
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0xFFFFFFF8u, 0u, 0u, from, 1u, small));
  CHECK_EQ(scene.commands.texture_copies, 2u); }

TEST(d3d12_session_isolation, a_texture_copy_that_reads_past_its_source_mip_is_refused)
{ Scene scene;
  scene.rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 64u, 64u);
  const CopyLocation to = subresource_of(texture);
  const CopyLocation from = subresource_of(other_texture);
  const Box wide = {0u, 0u, 0u, 17u, 1u, 1u};
  const Box tall = {0u, 0u, 0u, 1u, 17u, 1u};
  const Box deep = {0u, 0u, 0u, 1u, 1u, 2u};
  const Box inverted = {4u, 0u, 0u, 2u, 1u, 1u};
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 0u, from, 1u, wide));
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 0u, from, 1u, tall));
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 0u, from, 1u, deep));
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 0u, from, 1u, inverted));
  CHECK_EQ(scene.commands.texture_copies, 0u); }

TEST(d3d12_session_isolation, a_texture_copy_names_a_subresource_that_exists_in_a_texture)
{ Scene scene;
  CopyLocation to = subresource_of(texture);
  CopyLocation from = subresource_of(other_texture);
  to.subresource = 1u;
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 0u, from, 0u, Box{}));
  to.subresource = 0u;
  from.subresource = 1u;
  CHECK(!scene.session().on_CopyTextureRegion12(list, to, 0u, 0u, 0u, from, 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, subresource_of(target), 0u, 0u, 0u, subresource_of(source), 0u,
                                                Box{0u, 0u, 0u, 1u, 1u, 1u}));
  CHECK_EQ(scene.commands.texture_copies, 0u); }

TEST(d3d12_session_isolation, a_texture_copy_between_texels_of_different_sizes_or_samples_is_refused)
{ Scene scene;
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R16_UNORM, 16u, 16u);
  CHECK(!scene.session().on_CopyTextureRegion12(list, subresource_of(texture), 0u, 0u, 0u,
                                                subresource_of(other_texture), 0u, Box{}));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u, 4u);
  CHECK(!scene.session().on_CopyTextureRegion12(list, subresource_of(texture), 0u, 0u, 0u,
                                                subresource_of(other_texture), 0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 0u); }

TEST(d3d12_session_isolation, a_buffer_footprint_is_laid_out_in_a_format_of_the_typeless_group_of_the_texture_it_is_copied_with)
{ Scene scene;
  scene.dst.desc = buffer_desc(8192u);
  scene.src.desc = buffer_desc(8192u);
  const CopyLocation image = subresource_of(texture);
  CHECK(scene.session().on_CopyTextureRegion12(list, image, 0u, 0u, 0u, footprint_of(source, format_rgba, 16u, 16u), 0u,
                                               Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, format_rgba, 16u, 16u), 0u, 0u, 0u, image, 0u,
                                               Box{}));
  CHECK_EQ(scene.commands.texture_copies, 2u);
  CHECK(!scene.session().on_CopyTextureRegion12(list, image, 0u, 0u, 0u, footprint_of(source, format_r8, 16u, 16u), 0u,
                                                Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(target, format_r8, 16u, 16u), 0u, 0u, 0u, image, 0u,
                                                Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(target, format_r32, 16u, 16u), 0u, 0u, 0u, image, 0u,
                                                Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, format_rgba_uint, 16u, 16u), 0u, 0u, 0u, image,
                                               0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 3u);
  scene.rgba.desc.SampleDesc.Count = 4u;
  CHECK(!scene.session().on_CopyTextureRegion12(list, image, 0u, 0u, 0u, footprint_of(source, format_rgba, 16u, 16u), 0u,
                                                Box{}));
  CHECK_EQ(scene.commands.texture_copies, 3u); }

TEST(d3d12_session_isolation, a_footprint_on_a_texture_or_a_footprint_pair_is_refused)
{ Scene scene;
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(texture, format_rgba, 16u, 16u), 0u, 0u, 0u,
                                                subresource_of(other_texture), 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(target, format_rgba, 16u, 16u), 0u, 0u, 0u,
                                                footprint_of(source, format_rgba, 16u, 16u), 0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 0u); }

TEST(d3d12_session_isolation, a_heap_past_what_the_host_makes_answers_out_of_memory_and_leaves_the_device_healthy)
{ Scene scene;
  const HeapProperties device_local = {1u, 0u, 0u, 0u};
  std::uint32_t        answered = 0u;
  CHECK(av_d3d12_CreateHeap12(scene.rig, 40u, device_local, 0u, most_heap_bytes + 1u, 0u, 0u, 0u, &answered));
  CHECK_EQ(answered, out_of_memory);
  CHECK(av_d3d12_CreateHeap12(scene.rig, 41u, device_local, 0u, ~std::uint64_t{0}, 0u, 0u, 0u, &answered));
  CHECK_EQ(answered, out_of_memory);
  CHECK(scene.rig.objects().find(40u, D3dObjectKind::heap12) == nullptr);
  CHECK_EQ(scene.rig.refused(), 0u);
  std::uint32_t removed = 1u;
  CHECK(av_d3d12_Finish12(scene.rig, &removed));
  CHECK_EQ(removed, 0u);
  CHECK_EQ(scene.session().removed_reason(), 0u);
  }

TEST(d3d12_session_isolation, a_heap_no_guest_could_describe_still_removes_the_device)
{ Scene scene;
  std::uint32_t answered = 0u;
  CHECK(!av_d3d12_CreateHeap12(scene.rig, 40u, HeapProperties{0u, 0u, 0u, 0u}, 0u, 4096u, 0u, 0u, 0u, &answered));
  std::uint32_t removed = 0u;
  CHECK(av_d3d12_Finish12(scene.rig, &removed));
  CHECK_EQ(removed, driver_internal_error); }

namespace {

CopyLocation plane_of(std::uint32_t resource, std::uint32_t subresource)
{
    CopyLocation made = subresource_of(resource);
    made.subresource = subresource;
    return made;
}

}

TEST(d3d12_session_isolation, a_compressed_texture_is_copied_to_and_from_the_uncompressed_one_of_its_block_size)
{ Scene scene;
  scene.rgba.desc = texture_desc(DXGI_FORMAT_BC1_UNORM, 16u, 16u);
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R32G32_FLOAT, 4u, 4u);
  const CopyLocation compressed = subresource_of(texture);
  const CopyLocation encoded = subresource_of(other_texture);
  CHECK(scene.session().on_CopyTextureRegion12(list, encoded, 0u, 0u, 0u, compressed, 0u, Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, compressed, 0u, 0u, 0u, encoded, 0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 2u);
  const Box two_blocks = {0u, 0u, 0u, 8u, 8u, 1u};
  CHECK(scene.session().on_CopyTextureRegion12(list, encoded, 2u, 2u, 0u, compressed, 1u, two_blocks));
  CHECK(!scene.session().on_CopyTextureRegion12(list, encoded, 3u, 2u, 0u, compressed, 1u, two_blocks));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R32G32_FLOAT, 3u, 3u);
  CHECK(!scene.session().on_CopyTextureRegion12(list, encoded, 0u, 0u, 0u, compressed, 0u, Box{}));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R32G32B32A32_FLOAT, 4u, 4u);
  CHECK(!scene.session().on_CopyTextureRegion12(list, encoded, 0u, 0u, 0u, compressed, 0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 3u); }

TEST(d3d12_session_isolation, a_depth_stencil_plane_is_laid_out_in_the_format_of_that_plane_and_no_other)
{ Scene scene;
  scene.dst.desc = buffer_desc(8192u);
  scene.rgba.desc = texture_desc(DXGI_FORMAT_R24G8_TYPELESS, 16u, 16u);
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 16u, 16u);
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, 46u, 16u, 16u), 0u, 0u, 0u,
                                               plane_of(texture, 0u), 0u, Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, 60u, 16u, 16u), 0u, 0u, 0u,
                                               plane_of(texture, 1u), 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(target, 60u, 16u, 16u), 0u, 0u, 0u,
                                                plane_of(texture, 0u), 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(target, 46u, 16u, 16u), 0u, 0u, 0u,
                                                plane_of(texture, 1u), 0u, Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, 39u, 16u, 16u), 0u, 0u, 0u,
                                               plane_of(other_texture, 0u), 0u, Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, 62u, 16u, 16u), 0u, 0u, 0u,
                                               plane_of(other_texture, 1u), 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(target, 62u, 16u, 16u), 0u, 0u, 0u,
                                                plane_of(other_texture, 0u), 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, plane_of(texture, 0u), 0u, 0u, 0u, plane_of(other_texture, 1u),
                                                0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 4u); }

TEST(d3d12_session_isolation, a_planar_video_texture_is_copied_plane_by_plane)
{ Scene scene;
  scene.dst.desc = buffer_desc(8192u);
  scene.rgba.desc = texture_desc(DXGI_FORMAT_NV12, 16u, 16u);
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_NV12, 16u, 16u);
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, format_r8, 16u, 16u), 0u, 0u, 0u,
                                               plane_of(texture, 0u), 0u, Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, footprint_of(target, 49u, 8u, 8u), 0u, 0u, 0u,
                                               plane_of(texture, 1u), 0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, footprint_of(target, format_r8, 8u, 8u), 0u, 0u, 0u,
                                                plane_of(texture, 1u), 0u, Box{}));
  CHECK(scene.session().on_CopyTextureRegion12(list, plane_of(other_texture, 1u), 0u, 0u, 0u, plane_of(texture, 1u),
                                               0u, Box{}));
  CHECK(!scene.session().on_CopyTextureRegion12(list, plane_of(other_texture, 0u), 0u, 0u, 0u, plane_of(texture, 1u),
                                                0u, Box{}));
  CHECK_EQ(scene.commands.texture_copies, 3u); }

TEST(d3d12_session_isolation, a_query_must_name_a_slot_of_a_heap_of_its_kind)
{ Scene scene;
  CHECK(scene.session().on_BeginQuery12(list, queries, 0u, 15u));
  CHECK(!scene.session().on_BeginQuery12(list, queries, 0u, 16u));
  CHECK(!scene.session().on_EndQuery12(list, queries, 2u, 0u));
  CHECK(scene.session().on_EndQuery12(list, queries, 1u, 0u));
  CHECK(scene.session().on_ResolveQueryData12(list, queries, 0u, 8u, 8u, target, 0u));
  CHECK(!scene.session().on_ResolveQueryData12(list, queries, 0u, 8u, 9u, target, 0u));
  CHECK(!scene.session().on_ResolveQueryData12(list, queries, 2u, 0u, 1u, target, 0u));
  CHECK(!scene.session().on_ResolveQueryData12(list, queries, 0u, 0xFFFFFFFFu, 2u, target, 0u)); }

TEST(d3d12_session_isolation, a_resolve_names_subresources_of_one_size_and_samples_that_resolve)
{ Scene scene;
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u, 4u);
  CHECK(scene.session().on_ResolveSubresource12(list, texture, 0u, other_texture, 0u, format_rgba));
  CHECK(!scene.session().on_ResolveSubresource12(list, texture, 1u, other_texture, 0u, format_rgba));
  CHECK(!scene.session().on_ResolveSubresource12(list, texture, 0u, other_texture, 1u, format_rgba));
  CHECK(!scene.session().on_ResolveSubresource12(list, texture, 0u, other_texture, 0u, format_r8));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 32u, 16u, 4u);
  CHECK(!scene.session().on_ResolveSubresource12(list, texture, 0u, other_texture, 0u, format_rgba));
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u, 1u);
  CHECK(!scene.session().on_ResolveSubresource12(list, texture, 0u, other_texture, 0u, format_rgba));
  scene.rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u, 4u);
  scene.other_rgba.desc = texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u, 4u);
  CHECK(!scene.session().on_ResolveSubresource12(list, texture, 0u, other_texture, 0u, format_rgba)); }
