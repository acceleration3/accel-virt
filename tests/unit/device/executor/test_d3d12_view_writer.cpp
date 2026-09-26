/* Descriptors written from the wire's view descriptions: each kind's
 * dimension and union members as the guest's driver sends them, turned into
 * the Direct3D 12 description with the resource the guest's id names. The
 * device records what it was asked to write, so the description itself is
 * judged; a dimension the kind has not got, a member count past the wire's, a
 * resource id naming nothing and an unbound writer are each nothing written. */
#include "device/executor/d3d12_address_map.hpp"
#include "device/executor/d3d12_view_writer.hpp"
#include "device/executor/d3d_object_table.hpp"
#include "device/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::D3d12AddressMap;
using av::D3d12ViewWriter;
using av::D3dObjectKind;
using av::D3dObjectTable;
using ViewDesc = av::d3d12proto::ViewDesc;
using SamplerDesc = av::d3d12proto::SamplerDesc;

namespace {

enum class Made : std::uint8_t { nothing, srv, uav, rtv, dsv, cbv, sampler };

/* A device that keeps the last view it was asked to write. */
class RecordingDevice final : public ID3D12Device {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE   Release() override { return 1u; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE GetNodeCount() override { return 0u; }
    HRESULT STDMETHODCALLTYPE CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandList(UINT, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator *, ID3D12PipelineState *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D12_FEATURE, void *, UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE) override { return 0u; }
    HRESULT STDMETHODCALLTYPE CreateRootSignature(UINT, const void *, SIZE_T, REFIID, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE CopyDescriptors(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *, D3D12_DESCRIPTOR_HEAP_TYPE) override {}
    void STDMETHODCALLTYPE CopyDescriptorsSimple(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE, const D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE) override {}
    D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE GetResourceAllocationInfo(D3D12_RESOURCE_ALLOCATION_INFO * answer, UINT, UINT, const D3D12_RESOURCE_DESC *) override { return answer; }
    D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE GetCustomHeapProperties(D3D12_HEAP_PROPERTIES * answer, UINT, D3D12_HEAP_TYPE) override { return answer; }
    HRESULT STDMETHODCALLTYPE CreateCommittedResource(const D3D12_HEAP_PROPERTIES *, D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateHeap(const D3D12_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreatePlacedResource(ID3D12Heap *, UINT64, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateReservedResource(const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(ID3D12DeviceChild *, const SECURITY_ATTRIBUTES *, DWORD, const WCHAR *, HANDLE *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenSharedHandle(HANDLE, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenSharedHandleByName(const WCHAR *, DWORD, HANDLE *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE MakeResident(UINT, ID3D12Pageable *const *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Evict(UINT, ID3D12Pageable *const *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE GetCopyableFootprints(const D3D12_RESOURCE_DESC *, UINT, UINT, UINT64, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *, UINT *, UINT64 *, UINT64 *) override {}
    HRESULT STDMETHODCALLTYPE CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetStablePowerState(WINBOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *, ID3D12RootSignature *, REFIID, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE GetResourceTiling(ID3D12Resource *, UINT *, D3D12_PACKED_MIP_INFO *, D3D12_TILE_SHAPE *, UINT *, UINT, D3D12_SUBRESOURCE_TILING *) override {}
    LUID* STDMETHODCALLTYPE GetAdapterLuid(LUID * answer) override { return answer; }
    HRESULT STDMETHODCALLTYPE CreateFence(UINT64, D3D12_FENCE_FLAGS, REFIID, void **) override { return E_NOTIMPL; }

    void STDMETHODCALLTYPE CreateShaderResourceView(ID3D12Resource *viewed, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc,
                                                    D3D12_CPU_DESCRIPTOR_HANDLE handle) override
    {
        record(Made::srv, viewed, nullptr, desc != nullptr, handle);
        if (desc != nullptr) srv = *desc;
    }
    void STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D12Resource *viewed, ID3D12Resource *counted,
                                                     const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc,
                                                     D3D12_CPU_DESCRIPTOR_HANDLE handle) override
    {
        record(Made::uav, viewed, counted, desc != nullptr, handle);
        if (desc != nullptr) uav = *desc;
    }
    void STDMETHODCALLTYPE CreateRenderTargetView(ID3D12Resource *viewed, const D3D12_RENDER_TARGET_VIEW_DESC *desc,
                                                  D3D12_CPU_DESCRIPTOR_HANDLE handle) override
    {
        record(Made::rtv, viewed, nullptr, desc != nullptr, handle);
        if (desc != nullptr) rtv = *desc;
    }
    void STDMETHODCALLTYPE CreateDepthStencilView(ID3D12Resource *viewed, const D3D12_DEPTH_STENCIL_VIEW_DESC *desc,
                                                  D3D12_CPU_DESCRIPTOR_HANDLE handle) override
    {
        record(Made::dsv, viewed, nullptr, desc != nullptr, handle);
        if (desc != nullptr) dsv = *desc;
    }
    void STDMETHODCALLTYPE CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc,
                                                    D3D12_CPU_DESCRIPTOR_HANDLE handle) override
    {
        record(Made::cbv, nullptr, nullptr, true, handle);
        cbv = *desc;
    }
    void STDMETHODCALLTYPE CreateSampler(const D3D12_SAMPLER_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) override
    {
        record(Made::sampler, nullptr, nullptr, true, handle);
        sampler = *desc;
    }

    void forget() { made = Made::nothing; }

    Made                            made = Made::nothing;
    const void                     *resource = nullptr;
    const void                     *counter = nullptr;
    bool                            described = false;
    SIZE_T                          at = 0u;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    D3D12_RENDER_TARGET_VIEW_DESC   rtv = {};
    D3D12_DEPTH_STENCIL_VIEW_DESC   dsv = {};
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
    D3D12_SAMPLER_DESC              sampler = {};

private:
    void record(Made kind, const void *viewed, const void *counted, bool with_desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        made = kind;
        resource = viewed;
        counter = counted;
        described = with_desc;
        at = handle.ptr;
    }
};

class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE Release() override { return 1u; }
};

/* Most of a megabyte each: kept out of the stack. */
D3dObjectTable &session_objects()
{
    static D3dObjectTable table;
    return table;
}

D3d12AddressMap &session_addresses()
{
    static D3d12AddressMap map;
    return map;
}

constexpr std::uint32_t texture = 5u;    /* a Direct3D 12 resource */
constexpr std::uint32_t counter = 6u;    /* another */
constexpr std::uint32_t sampler_id = 7u; /* not a resource */
constexpr std::uint32_t nobody = 9u;

/* The writer over a session's tables, bound to a recording device. */
class Rig {
public:
    Rig() : writer_(session_objects(), session_addresses())
    {
        objects().clear();
        addresses().clear();
        static_cast<void>(objects().insert(texture, D3dObjectKind::resource12, &texture_));
        static_cast<void>(objects().insert(counter, D3dObjectKind::resource12, &counter_));
        static_cast<void>(objects().insert(sampler_id, D3dObjectKind::sampler, &other_));
        static_cast<void>(addresses().insert(0x100000u, 0x10000u, 0xABC00000u, 1u));
        writer_.bind(&device_);
    }
    ~Rig()
    {
        objects().clear();
        addresses().clear();
    }
    Rig(const Rig &) = delete;
    Rig &operator=(const Rig &) = delete;

    D3d12ViewWriter &writer() { return writer_; }
    RecordingDevice &device() { return device_; }
    const void      *texture_object() const { return &texture_; }
    const void      *counter_object() const { return &counter_; }

private:
    D3dObjectTable  &objects() { return session_objects(); }
    D3d12AddressMap &addresses() { return session_addresses(); }

    Counted         texture_, counter_, other_;
    RecordingDevice device_;
    D3d12ViewWriter writer_;
};

ViewDesc view(std::uint32_t resource, std::uint32_t dimension, std::uint32_t format = 28u)
{
    ViewDesc desc = {};
    desc.resource = resource;
    desc.dimension = dimension;
    desc.format = format;
    return desc;
}

}  // namespace

TEST(d3d12_view_writer_bind, an_unbound_writer_writes_nothing)
{ Rig rig;
  rig.writer().bind(nullptr);
  const ViewDesc desc = view(texture, 4u);
  CHECK(!rig.writer().shader_resource(64u, true, desc));
  CHECK(!rig.writer().unordered_access(64u, true, desc));
  CHECK(!rig.writer().render_target(64u, true, desc));
  CHECK(!rig.writer().depth_stencil(64u, true, desc));
  CHECK(!rig.writer().constant_buffer(64u, 0u, 256u));
  CHECK(!rig.writer().sampler(64u, SamplerDesc{}));
  CHECK(rig.device().made == Made::nothing); }

/* A null description is the resource's own format and whole extent: it needs
 * a resource, and names it. */
TEST(d3d12_view_writer_shader_resource, a_null_description_views_the_whole_resource)
{ Rig rig;
  CHECK(rig.writer().shader_resource(0x40u, false, view(texture, 0u)));
  CHECK(rig.device().made == Made::srv);
  CHECK(!rig.device().described);
  CHECK(rig.device().resource == rig.texture_object());
  CHECK_EQ(rig.device().at, 0x40u);
  rig.device().forget();
  CHECK(!rig.writer().shader_resource(0x40u, false, view(0u, 0u)));
  CHECK(!rig.writer().unordered_access(0x40u, false, view(0u, 0u)));
  CHECK(!rig.writer().render_target(0x40u, false, view(0u, 0u)));
  CHECK(!rig.writer().depth_stencil(0x40u, false, view(0u, 0u)));
  CHECK(rig.device().made == Made::nothing); }

TEST(d3d12_view_writer_shader_resource, a_texture2d_s_members_and_its_lod_clamp_land_in_the_union)
{ Rig rig;
  ViewDesc desc = view(texture, 4u /* D3D12_SRV_DIMENSION_TEXTURE2D */);
  desc.mapping = 0x1688u;
  desc.args[0] = 1u;   /* MostDetailedMip */
  desc.args[1] = 2u;   /* MipLevels */
  desc.args[2] = 3u;   /* PlaneSlice */
  desc.args[3] = 99u;  /* not a member of this dimension */
  desc.min_lod = 0.5f;
  CHECK(rig.writer().shader_resource(0x80u, true, desc));
  const D3D12_SHADER_RESOURCE_VIEW_DESC &made = rig.device().srv;
  CHECK(rig.device().described);
  CHECK_EQ(static_cast<std::uint32_t>(made.Format), 28u);
  CHECK_EQ(static_cast<std::uint32_t>(made.ViewDimension), 4u);
  CHECK_EQ(made.Shader4ComponentMapping, 0x1688u);
  CHECK_EQ(made.Texture2D.MostDetailedMip, 1u);
  CHECK_EQ(made.Texture2D.MipLevels, 2u);
  CHECK_EQ(made.Texture2D.PlaneSlice, 3u);
  CHECK(made.Texture2D.ResourceMinLODClamp == 0.5f); }

TEST(d3d12_view_writer_shader_resource, a_buffer_takes_its_first_element_count_stride_and_flags)
{ Rig rig;
  ViewDesc desc = view(texture, 1u, 0u);
  desc.first_element = 0x100000000ull;
  desc.args[0] = 64u;
  desc.args[1] = 16u;
  desc.flags = 1u;
  CHECK(rig.writer().shader_resource(0u, true, desc));
  const D3D12_SHADER_RESOURCE_VIEW_DESC &made = rig.device().srv;
  CHECK(made.Buffer.FirstElement == 0x100000000ull);
  CHECK_EQ(made.Buffer.NumElements, 64u);
  CHECK_EQ(made.Buffer.StructureByteStride, 16u);
  CHECK_EQ(static_cast<std::uint32_t>(made.Buffer.Flags), 1u); }

TEST(d3d12_view_writer_shader_resource, refuses_an_unknown_dimension_format_or_resource)
{ Rig rig;
  CHECK(!rig.writer().shader_resource(0u, true, view(texture, 0u)));     /* UNKNOWN */
  CHECK(!rig.writer().shader_resource(0u, true, view(texture, 11u)));    /* past the last */
  CHECK(!rig.writer().shader_resource(0u, true, view(texture, 4u, 192u)));
  CHECK(!rig.writer().shader_resource(0u, true, view(nobody, 4u)));
  CHECK(!rig.writer().shader_resource(0u, true, view(sampler_id, 4u)));   /* not a resource */
  CHECK(rig.device().made == Made::nothing);
  /* No resource with a description is a null descriptor, which is written. */
  CHECK(rig.writer().shader_resource(0u, true, view(0u, 10u, 191u)));
  CHECK(rig.device().resource == nullptr); }

TEST(d3d12_view_writer_unordered_access, names_its_counter_and_writes_the_counter_offset)
{ Rig rig;
  ViewDesc desc = view(texture, 1u, 0u);
  desc.counter_resource = counter;
  desc.first_element = 8u;
  desc.counter_offset = 4096u;
  desc.args[0] = 32u;
  desc.args[1] = 4u;
  CHECK(rig.writer().unordered_access(0x10u, true, desc));
  CHECK(rig.device().made == Made::uav);
  CHECK(rig.device().counter == rig.counter_object());
  CHECK(rig.device().uav.Buffer.FirstElement == 8u);
  CHECK_EQ(rig.device().uav.Buffer.NumElements, 32u);
  CHECK_EQ(rig.device().uav.Buffer.StructureByteStride, 4u);
  CHECK(rig.device().uav.Buffer.CounterOffsetInBytes == 4096u);
  ViewDesc texture2d = view(texture, 4u);
  texture2d.args[0] = 3u;   /* MipSlice */
  texture2d.args[1] = 1u;   /* PlaneSlice */
  CHECK(rig.writer().unordered_access(0x20u, true, texture2d));
  CHECK_EQ(rig.device().uav.Texture2D.MipSlice, 3u);
  CHECK_EQ(rig.device().uav.Texture2D.PlaneSlice, 1u);
  CHECK(rig.device().counter == nullptr);
  CHECK(rig.writer().unordered_access(0x30u, false, desc));   /* null description, still its counter */
  CHECK(!rig.device().described);
  CHECK(rig.device().counter == rig.counter_object()); }

TEST(d3d12_view_writer_unordered_access, refuses_a_counter_naming_nothing_and_dimensions_it_has_not_got)
{ Rig rig;
  ViewDesc desc = view(texture, 4u);
  desc.counter_resource = nobody;
  CHECK(!rig.writer().unordered_access(0u, true, desc));
  CHECK(!rig.writer().unordered_access(0u, true, view(texture, 6u)));   /* no UAV of a cube */
  CHECK(!rig.writer().unordered_access(0u, true, view(texture, 7u)));
  CHECK(!rig.writer().unordered_access(0u, true, view(texture, 9u)));
  CHECK(!rig.writer().unordered_access(0u, true, view(nobody, 4u)));
  CHECK(!rig.writer().unordered_access(0u, true, view(texture, 4u, 200u)));
  CHECK(rig.device().made == Made::nothing); }

TEST(d3d12_view_writer_render_target, writes_a_texture2d_or_a_buffer_and_refuses_the_rest)
{ Rig rig;
  ViewDesc desc = view(texture, 4u);
  desc.args[0] = 2u;
  desc.args[1] = 1u;
  CHECK(rig.writer().render_target(0x100u, true, desc));
  CHECK(rig.device().made == Made::rtv);
  CHECK_EQ(rig.device().rtv.Texture2D.MipSlice, 2u);
  CHECK_EQ(rig.device().rtv.Texture2D.PlaneSlice, 1u);
  CHECK_EQ(rig.device().at, 0x100u);
  ViewDesc buffer = view(texture, 1u);
  buffer.first_element = 12u;
  buffer.args[0] = 40u;
  CHECK(rig.writer().render_target(0x100u, true, buffer));
  CHECK(rig.device().rtv.Buffer.FirstElement == 12u);
  CHECK_EQ(rig.device().rtv.Buffer.NumElements, 40u);
  CHECK(rig.writer().render_target(0x140u, false, view(texture, 0u)));
  CHECK(!rig.device().described);
  rig.device().forget();
  CHECK(!rig.writer().render_target(0u, true, view(texture, 9u)));
  CHECK(!rig.writer().render_target(0u, true, view(texture, 0u)));
  CHECK(!rig.writer().render_target(0u, true, view(nobody, 4u)));
  CHECK(!rig.writer().render_target(0u, true, view(texture, 4u, 192u)));
  CHECK(rig.device().made == Made::nothing); }

TEST(d3d12_view_writer_depth_stencil, writes_its_flags_and_members_and_refuses_what_it_has_not_got)
{ Rig rig;
  ViewDesc desc = view(texture, 3u /* D3D12_DSV_DIMENSION_TEXTURE2D */, 45u);
  desc.flags = 2u;
  desc.args[0] = 1u;
  CHECK(rig.writer().depth_stencil(0x200u, true, desc));
  CHECK(rig.device().made == Made::dsv);
  CHECK_EQ(static_cast<std::uint32_t>(rig.device().dsv.Flags), 2u);
  CHECK_EQ(rig.device().dsv.Texture2D.MipSlice, 1u);
  CHECK_EQ(static_cast<std::uint32_t>(rig.device().dsv.Format), 45u);
  CHECK(rig.writer().depth_stencil(0x200u, false, view(texture, 0u)));
  CHECK(!rig.device().described);
  rig.device().forget();
  CHECK(!rig.writer().depth_stencil(0u, true, view(texture, 0u)));
  CHECK(!rig.writer().depth_stencil(0u, true, view(texture, 7u)));
  CHECK(!rig.writer().depth_stencil(0u, true, view(nobody, 3u)));
  CHECK(!rig.writer().depth_stencil(0u, true, view(texture, 3u, 192u)));
  CHECK(rig.device().made == Made::nothing); }

/* A constant buffer is 256-byte aligned and at most 64 KiB; its guest address
 * is the host buffer's plus the same offset, and one in no buffer is refused. */
TEST(d3d12_view_writer_constant_buffer, translates_the_guest_address_and_refuses_what_d3d12_would)
{ Rig rig;
  CHECK(rig.writer().constant_buffer(0x300u, 0x100200u, 512u));
  CHECK(rig.device().made == Made::cbv);
  CHECK(rig.device().cbv.BufferLocation == 0xABC00200u);
  CHECK_EQ(rig.device().cbv.SizeInBytes, 512u);
  CHECK(rig.writer().constant_buffer(0x300u, 0u, 256u));   /* a null view */
  CHECK(rig.device().cbv.BufferLocation == 0u);
  rig.device().forget();
  CHECK(!rig.writer().constant_buffer(0u, 0x100000u, 255u));
  CHECK(!rig.writer().constant_buffer(0u, 0x100000u, 65536u + 256u));
  CHECK(!rig.writer().constant_buffer(0u, 0x200000u, 256u));   /* in no buffer */
  CHECK(!rig.writer().constant_buffer(0u, 0x10FF00u, 512u));   /* running past its buffer */
  CHECK(rig.device().made == Made::nothing);
  CHECK(rig.writer().constant_buffer(0u, 0x100000u, 65536u));   /* the largest */ }

TEST(d3d12_view_writer_sampler, carries_every_field_of_the_wire_s_description)
{ Rig rig;
  SamplerDesc desc = {};
  desc.filter = 0x55u;
  desc.address_u = 1u;
  desc.address_v = 2u;
  desc.address_w = 3u;
  desc.mip_lod_bias = -1.5f;
  desc.max_anisotropy = 8u;
  desc.comparison = 4u;
  desc.border_color[0] = 0.25f;
  desc.border_color[3] = 1.0f;
  desc.min_lod = 1.0f;
  desc.max_lod = 9.0f;
  CHECK(rig.writer().sampler(0x400u, desc));
  const D3D12_SAMPLER_DESC &made = rig.device().sampler;
  CHECK_EQ(static_cast<std::uint32_t>(made.Filter), 0x55u);
  CHECK_EQ(static_cast<std::uint32_t>(made.AddressU), 1u);
  CHECK_EQ(static_cast<std::uint32_t>(made.AddressV), 2u);
  CHECK_EQ(static_cast<std::uint32_t>(made.AddressW), 3u);
  CHECK(made.MipLODBias == -1.5f);
  CHECK_EQ(made.MaxAnisotropy, 8u);
  CHECK_EQ(static_cast<std::uint32_t>(made.ComparisonFunc), 4u);
  CHECK(made.BorderColor[0] == 0.25f && made.BorderColor[3] == 1.0f);
  CHECK(made.MinLOD == 1.0f && made.MaxLOD == 9.0f);
  CHECK_EQ(rig.device().at, 0x400u); }
