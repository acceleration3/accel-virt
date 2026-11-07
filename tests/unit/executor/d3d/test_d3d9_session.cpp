#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d9_session.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "core/log/driver_log.hpp"
#include "executor/d3d/slice_storages.hpp"
#include "executor/eventfd_wake.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "executor/d3d/slice_bindings.hpp"
#include "proto/d3d/buffer_slice_support.hpp"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::D3d9Session;
using namespace av::d3d9proto;

namespace av {

struct D3d9SessionProbe {
    static bool refuse(D3d9Session &session, std::uint32_t query, std::uint32_t hresult)
    {
        return session.refused_.note(query, hresult);
    }
    static std::uint32_t refused(const D3d9Session &session) { return session.refused_.count(); }
};

struct WatchedBlocksProbe {
    static void draw_from(WatchedBlocks &blocks, BlockSource &source) { blocks.blocks_ = &source; }
};

}

namespace {

constexpr std::uint32_t nobody = 77u;
constexpr std::uint32_t not_available = 0x8876086au;
constexpr std::uint32_t invalid_call = 0x8876086cu;

av::HostHeap                          heap;
av::WatchedBlocks                     blocks(heap);
av::ObjectStorage<av::D3dObjectTable> table_storage;
av::ObjectStorage<D3d9Session> session_storage;
alignas(64) std::uint8_t guest[4096];

class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }

    ULONG references = 1u;
};

class FreshSession {
public:
    FreshSession() : sink_(), lines_(sink_, "d3d9: "), slices_(blocks, lines_), wake_()
    {
        CHECK_STATUS(bytes_.bind(0u, guest, sizeof guest), av::Status::ok);
        CHECK_STATUS(table_storage.construct(blocks), av::Status::ok);
        CHECK_STATUS(session_storage.construct(bytes_, table_storage.object(), blocks, slices_, wake_, lines_),
                     av::Status::ok);
        session_storage.object().initialize(nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    ~FreshSession()
    {
        session_storage.object().shut_down();
        session_storage.destroy();
        table_storage.destroy();
    }
    FreshSession(const FreshSession &) = delete;
    FreshSession &operator=(const FreshSession &) = delete;

    D3d9Session        &session() { return session_storage.object(); }
    av::D3dObjectTable &table() { return table_storage.object(); }

private:
    av::RecordingLog sink_;
    av::DriverLog    lines_;
    av::SliceStorages slices_;
    av::EventfdWake  wake_;
    av::GuestBytes   bytes_;
};

}

TEST(d3d9_session_knows, its_own_range_and_no_other)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    CHECK(session.knows(kCmd_CreateDevice9));
    CHECK(session.knows(kCmd_Clear9));
    CHECK(!session.knows(kCmd_CreateDevice9 - 1u));
    CHECK(!session.knows(0x00040001u));
    CHECK(!session.knows(0x00041000u));
}

TEST(d3d9_session_on_CreateDevice9, a_gpu_with_no_d3d9_makes_no_device)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    std::uint32_t hresult = 0u;
    CHECK(session.on_CreateDevice9(1u, 0u, hresult));
    CHECK_EQ(hresult, not_available);
    CHECK(session.on_CreateDevice9(0u, 0u, hresult));
    CHECK_EQ(hresult, invalid_call);
    CHECK(session.invariants_hold());
    CHECK(!session.on_SelectDevice9(1u));
    CHECK(session.on_Finish9(hresult));
    CHECK_EQ(hresult, invalid_call);

    std::uint8_t        caps[512] = {};
    std::uint64_t       caps_size = 1u, rows_size = 1u;
    const std::uint32_t formats[1] = {21u};
    std::uint8_t        rows[64] = {};
    CHECK(session.on_QueryCaps9(1u, formats, hresult, caps, sizeof caps, caps_size, rows, sizeof rows, rows_size));
    CHECK_EQ(hresult, not_available);
    CHECK_EQ(caps_size, 0u);
    CHECK_EQ(rows_size, 0u);
}

TEST(d3d9_session_on_CreateDevice9, a_device_there_is_no_memory_to_keep_is_out_of_memory)
{
    av::MockPlatform platform;
    av::WatchedBlocksProbe::draw_from(blocks, platform);
    {
        FreshSession fresh;
        D3d9Session &session = fresh.session();
        std::uint32_t hresult = 0u;
        platform.fail_allocation_at(1u);
        CHECK(session.on_CreateDevice9(1u, 0u, hresult));
        CHECK_EQ(hresult, static_cast<std::uint32_t>(E_OUTOFMEMORY));
        CHECK(session.on_CreateDevice9(1u, 0u, hresult));
        CHECK_EQ(hresult, not_available);
        CHECK(session.invariants_hold());
    }
    av::WatchedBlocksProbe::draw_from(blocks, heap);
}

TEST(d3d9_session_on_commands, with_no_device_selected_each_is_refused)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    const Rect   rect = {0, 0, 4, 4};
    const Box    box = {0u, 0u, 0u, 4u, 4u, 1u};
    const std::uint8_t data[64] = {};
    const float        values[16] = {};
    const std::int32_t integers[4] = {};
    const std::uint32_t words[4] = {7u, 1u, 0u, 0u};
    const VertexElement element = {0u, 0u, 3u, 0u, 0u, 0u};
    CHECK(session.on_Flush9());
    CHECK(session.invariants_hold());
    CHECK(!session.on_Destroy9(nobody));
    CHECK(!session.on_CreateSurface9(1u, 1u, 4u, 4u, 21u, 0u, 0u, 0u));
    CHECK(!session.on_CreateTexture9(1u, 1u, 4u, 4u, 1u, 1u, 0u, 21u, 1u));
    CHECK(!session.on_CreateVertexBuffer9(1u, 64u, 0u, 0u, 0u));
    CHECK(!session.on_CreateIndexBuffer9(1u, 64u, 0u, 101u, 0u));
    CHECK(!session.on_WriteTexture9(nobody, 0u, box, 16u, 0u, 0u, data, sizeof data));
    CHECK(!session.on_WriteBuffer9(nobody, 0u, 0u, data, sizeof data));
    CHECK(!session.on_GenerateMips9(nobody, 2u));
    CHECK(!session.on_StretchRect9(nobody, 0u, rect, nobody, 0u, rect, 2u));
    CHECK(!session.on_ColorFill9(nobody, 0u, rect, 0u));
    CHECK(!session.on_PresentPoint9(nobody, 0u, 0x9152u, 1u));
    CHECK(!session.on_CreateVertexShader9(1u, data, sizeof data));
    CHECK(!session.on_CreatePixelShader9(1u, data, sizeof data));
    CHECK(!session.on_CreateVertexDeclaration9(1u, 1u, &element));
    CHECK(!session.on_SetVertexShader9(0u));
    CHECK(!session.on_SetPixelShader9(0u));
    CHECK(!session.on_SetVertexDeclaration9(0u));
    CHECK(!session.on_SetFvf9(0x44u));
    CHECK(!session.on_SetConstantsF9(0u, 0u, 16u, values));
    CHECK(!session.on_SetConstantsI9(0u, 0u, 4u, integers));
    CHECK(!session.on_SetConstantsB9(0u, 0u, 4u, words));
    CHECK(!session.on_SetRenderStates9(2u, words));
    CHECK(!session.on_SetSamplerStates9(3u, words));
    CHECK(!session.on_SetTextureStageStates9(3u, words));
    CHECK(!session.on_SetTexture9(0u, 0u));
    CHECK(!session.on_SetStreamSource9(0u, 0u, 0u, 0u));
    CHECK(!session.on_SetStreamSourceFreq9(0u, 1u));
    CHECK(!session.on_SetIndices9(0u));
    CHECK(!session.on_SetViewport9(Viewport{0u, 0u, 4u, 4u, 0.0f, 1.0f}));
    CHECK(!session.on_SetScissorRect9(rect));
    CHECK(!session.on_SetDepthStencil9(0u, 0u));
    CHECK(!session.on_SetTransform9(2u, 16u, values));
    CHECK(!session.on_SetMaterial9(Material{}));
    Light light = {};
    light.type = 3u;
    CHECK(!session.on_SetLight9(0u, light));
    CHECK(!session.on_LightEnable9(0u, 1u));
    CHECK(!session.on_SetClipPlane9(0u, 4u, values));
    CHECK(!session.on_DrawPrimitive9(4u, 0u, 1u));
    CHECK(!session.on_DrawIndexedPrimitive9(4u, 0, 0u, 3u, 0u, 1u));
    CHECK(!session.on_CreateQuery9(1u, 8u));
    CHECK(!session.on_IssueQuery9(nobody, 1u));
    CHECK(!session.on_SetRenderTarget9(0u, nobody, 0u));
    CHECK(!session.on_Clear9(0u, nullptr, 1u, 0u, 1.0f, 0u));
    CHECK(session.invariants_hold());
}

TEST(d3d9_session_on_ReadSurface9, of_nothing_answers_invalid_call_and_writes_nothing)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    std::uint32_t hresult = 0u, width = 0u, height = 0u, format = 0u, pitch = 0u;
    std::uint8_t  pixels[64] = {};
    std::uint64_t written = 1u;
    CHECK(session.on_ReadSurface9(nobody, 0u, hresult, width, height, format, pitch, pixels, sizeof pixels, written));
    CHECK_EQ(hresult, invalid_call);
    CHECK_EQ(written, 0u);
    std::uint64_t data_size = 1u;
    CHECK(session.on_GetQueryData9(nobody, 1u, hresult, pixels, sizeof pixels, data_size));
    CHECK_EQ(hresult, invalid_call);
    CHECK_EQ(data_size, 0u);
}

TEST(d3d9_session_run, a_refusal_with_no_device_fails_the_session)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    std::uint8_t     frame[64] = {};
    std::uint8_t     answer[64] = {};
    std::uint8_t     scratch[256] = {};
    av::WireArena    arena(scratch, sizeof scratch);
    av::WireReader   indices(frame, sizeof(std::uint32_t));
    av::WireWriter   indices_reply(answer, sizeof answer);
    CHECK(!session.run(kCmd_SetIndices9, indices, indices_reply, arena));
    av::WireReader flush(frame, 0u);
    av::WireWriter flush_reply(answer, sizeof answer);
    CHECK(session.run(kCmd_Flush9, flush, flush_reply, arena));
    av::WireReader finish(frame, 0u);
    av::WireWriter finish_reply(answer, sizeof answer);
    CHECK(session.run(kCmd_Finish9, finish, finish_reply, arena));
    CHECK(finish_reply.offset() != 0u);
}

TEST(d3d9_session_shut_down, lets_go_of_direct3d_9_objects_and_keeps_the_rest)
{
    FreshSession fresh;
    Counted      eleven, nine;
    CHECK_STATUS(fresh.table().insert(5u, av::D3dObjectKind::resource, &eleven), av::Status::ok);
    CHECK_STATUS(fresh.table().insert(6u, av::D3dObjectKind::texture9, &nine), av::Status::ok);
    fresh.session().shut_down();
    CHECK_EQ(nine.references, 0u);
    CHECK_EQ(eleven.references, 1u);
    av::D3dObjectKind kind = av::D3dObjectKind::none;
    CHECK(fresh.table().find_any(5u, kind) == &eleven);
    CHECK(fresh.table().find_any(6u, kind) == nullptr);
    CHECK(fresh.session().invariants_hold());
    fresh.table().release_all();
}

TEST(d3d9_session_on_Destroy9, removes_only_direct3d_9_objects)
{
    FreshSession fresh;
    Counted      eleven, nine;
    CHECK_STATUS(fresh.table().insert(31u, av::D3dObjectKind::resource, &eleven), av::Status::ok);
    CHECK_STATUS(fresh.table().insert(32u, av::D3dObjectKind::surface9, &nine), av::Status::ok);
    CHECK(!fresh.session().on_Destroy9(31u));
    CHECK(fresh.table().find(31u, av::D3dObjectKind::resource) == &eleven);
    CHECK(fresh.session().on_Destroy9(32u));
    CHECK(fresh.table().find(32u, av::D3dObjectKind::surface9) == nullptr);
    CHECK_EQ(nine.references, 0u);
    fresh.table().remove(31u);
}

TEST(d3d9_session_slices, a_session_with_no_device_offers_no_buffer_slices_and_binds_none)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    std::uint32_t supported = 7u;
    CHECK(session.on_QueryBufferSlices9(supported));
    CHECK_EQ(supported, static_cast<std::uint32_t>(av::BufferSliceSupport::none));
    supported = 7u;
    CHECK(session.on_QueryDeviceSlices9(supported));
    CHECK_EQ(supported, static_cast<std::uint32_t>(av::BufferSliceSupport::none));
    CHECK(!session.on_BindBufferSlice9(nobody, 0x40u, 0u, 256u));
    CHECK(!session.on_BindBufferSlice9(nobody, 0u, 0u, 0u));
    CHECK(session.invariants_hold());
}

TEST(d3d9_session_slices, discarding_a_slice_no_lane_holds_changes_nothing)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    const std::uint8_t              bytes[16] = {};
    av::SliceBindings::Binding bound = {};
    bound.resource_ = nobody;
    bound.slice_ = bytes;
    bound.bytes_ = sizeof bytes;
    session.discard_slice(bound);
    CHECK(session.invariants_hold());
}

TEST(d3d9_session_queries, a_query_the_host_refused_to_create_answers_its_failure_when_asked_and_does_not_remove_the_device)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    CHECK(av::D3d9SessionProbe::refuse(session, 40u, not_available));
    std::uint32_t hresult = 0u;
    std::uint8_t  data[16] = {1u};
    std::uint64_t size = 9u;
    CHECK(session.on_GetQueryData9(40u, 1u, hresult, data, sizeof data, size));
    CHECK_EQ(hresult, not_available);
    CHECK_EQ(size, 0u);
    CHECK(session.on_GetQueryData9(40u, 1u, hresult, data, sizeof data, size));
    CHECK_EQ(hresult, not_available);
    CHECK(session.invariants_hold());
}

TEST(d3d9_session_queries, issuing_a_refused_query_is_ignored_and_a_bad_issue_flag_still_removes_the_device)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    CHECK(av::D3d9SessionProbe::refuse(session, 40u, not_available));
    CHECK(session.on_IssueQuery9(40u, 1u));
    CHECK(session.on_IssueQuery9(40u, 2u));
    CHECK(!session.on_IssueQuery9(40u, 7u));
    CHECK(!session.on_IssueQuery9(41u, 1u));
}

TEST(d3d9_session_queries, destroying_a_refused_query_forgets_it_and_a_second_destroy_is_refused)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    CHECK(av::D3d9SessionProbe::refuse(session, 40u, not_available));
    CHECK(session.on_Destroy9(40u));
    CHECK_EQ(av::D3d9SessionProbe::refused(session), 0u);
    CHECK(!session.on_Destroy9(40u));
    std::uint32_t hresult = 0u;
    std::uint8_t  data[16] = {};
    std::uint64_t size = 0u;
    CHECK(session.on_GetQueryData9(40u, 1u, hresult, data, sizeof data, size));
    CHECK_EQ(hresult, invalid_call);
}

TEST(d3d9_session_queries, a_session_that_shuts_down_forgets_the_refused_queries)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    CHECK(av::D3d9SessionProbe::refuse(session, 40u, not_available));
    session.shut_down();
    CHECK_EQ(av::D3d9SessionProbe::refused(session), 0u);
}
