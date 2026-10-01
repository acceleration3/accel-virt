/* The Direct3D 9 half of a Direct3D session with no Direct3D 9 behind it - a
 * host without DXVK's d3d9, or a GPU it does not run on - and a guest naming
 * devices and objects this session never made. Every asynchronous command is
 * refused, every synchronous one answered as a device that is not there, and
 * nothing is made: a handler that trusted a guest's id would hand DXVK a null
 * device. What a session does with a device is judged on the host GPU in
 * test_d3d9_session_device.cpp. */
#include "core/support/object_storage.hpp"
#include "device/executor/d3d9_session.hpp"
#include "device/executor/guest_region.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::D3d9Session;
using av::GuestRegion;
using namespace av::d3d9proto;

namespace {

constexpr std::uint32_t nobody = 77u;
constexpr std::uint32_t not_available = 0x8876086au;   /* D3DERR_NOTAVAILABLE */
constexpr std::uint32_t invalid_call = 0x8876086cu;    /* D3DERR_INVALIDCALL */

av::ObjectStorage<D3d9Session> session_storage;
alignas(64) std::uint8_t guest[4096];

class FreshSession {
public:
    FreshSession()
    {
        region_.take(guest, sizeof guest);
        CHECK_STATUS(session_storage.construct(region_), av::Status::ok);
        session_storage.object().initialize(nullptr);
    }
    ~FreshSession()
    {
        session_storage.object().shut_down();
        session_storage.destroy();
    }
    FreshSession(const FreshSession &) = delete;
    FreshSession &operator=(const FreshSession &) = delete;

    D3d9Session &session() { return session_storage.object(); }

private:
    GuestRegion region_;
};

}  // namespace

TEST(d3d9_session_guest_bytes, are_the_session_s_region_and_nothing_past_it)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    CHECK(session.guest_bytes(0u, 16u) == guest);
    CHECK(session.guest_bytes_out(16u, 16u) == guest + 16u);
    CHECK(session.guest_bytes(sizeof guest - 4u, 8u) == nullptr);
    CHECK(session.guest_bytes_out(sizeof guest, 1u) == nullptr);
    CHECK(session.invariants_hold());
}

TEST(d3d9_session_knows, its_own_range_and_no_other)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    CHECK(session.knows(kCmd_CreateDevice9));
    CHECK(session.knows(kCmd_Clear9));
    CHECK(!session.knows(kCmd_CreateDevice9 - 1u));
    CHECK(!session.knows(0x00040001u));   /* Direct3D 11's CreateDevice11 */
    CHECK(!session.knows(0x00041000u));   /* Direct3D 12's OpenDevice12 */
}

/* A GPU with no Direct3D 9 makes no device, says why, and answers every
 * adapter question as not available rather than with a caps of zeros. */
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

    std::uint8_t  caps[512] = {};
    std::uint64_t caps_size = 1u;
    std::uint32_t vendor = 0u, device = 0u, subsystem = 0u, revision = 0u, memory = 0u;
    CHECK(session.on_QueryCaps9(hresult, vendor, device, subsystem, revision, memory, caps, sizeof caps, caps_size));
    CHECK_EQ(hresult, not_available);
    CHECK_EQ(caps_size, 0u);
    std::uint32_t levels = 9u;
    CHECK(session.on_QueryMultisample9(21u, 4u, hresult, levels));
    CHECK_EQ(hresult, not_available);
    CHECK_EQ(levels, 0u);
    const std::uint32_t formats[1] = {21u};
    std::uint8_t        support[8] = {};
    std::uint64_t       support_size = 1u;
    CHECK(!session.on_QueryFormats9(1u, formats, support, sizeof support, support_size));
    CHECK_EQ(support_size, 0u);
}

/* With no device selected every command that would draw or make something is
 * refused: there is no device to hand DXVK, and nobody to remove. */
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
    CHECK(!session.on_Flush9());
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

/* A read or a query of nothing is answered - the guest is waiting - as an
 * invalid call, with nothing written. */
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

/* Run straight through the generated decoder: a refused asynchronous command
 * with no device selected fails the session, a synchronous one is answered. */
TEST(d3d9_session_run, a_refusal_with_no_device_fails_the_session)
{
    FreshSession fresh;
    D3d9Session &session = fresh.session();
    std::uint8_t     frame[64] = {};
    std::uint8_t     answer[64] = {};
    std::uint8_t     scratch[256] = {};
    av::WireArena    arena(scratch, sizeof scratch);
    av::WireReader   flush(frame, 0u);
    av::WireWriter   flush_reply(answer, sizeof answer);
    CHECK(!session.run(kCmd_Flush9, flush, flush_reply, arena));
    av::WireReader finish(frame, 0u);
    av::WireWriter finish_reply(answer, sizeof answer);
    CHECK(session.run(kCmd_Finish9, finish, finish_reply, arena));
    CHECK(finish_reply.offset() != 0u);
}
