#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d9_session.hpp"
#include "executor/d3d/d3d9_shader_tokens.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/wire/wire_arena.hpp"

#include <string.h>

namespace av {

namespace {

constexpr uint32_t stage_vertex = 0u;
constexpr uint32_t stage_pixel = 1u;
constexpr uint32_t vertex_float_registers = 256u;
constexpr uint32_t pixel_float_registers = 224u;
constexpr uint32_t integer_registers = 16u;
constexpr uint32_t boolean_registers = 16u;
constexpr uint32_t components = 4u;
constexpr uint32_t pair = 2u;
constexpr uint32_t triple = 3u;
constexpr uint32_t max_render_state = 255u;
constexpr uint32_t max_sampler_state = D3DSAMP_DMAPOFFSET;
constexpr uint32_t max_stage_state = D3DTSS_CONSTANT;
constexpr uint32_t pixel_samplers = 16u;
constexpr uint32_t first_vertex_sampler = D3DDMAPSAMPLER;
constexpr uint32_t last_vertex_sampler = D3DVERTEXTEXTURESAMPLER3;
constexpr uint32_t texture_stages = 8u;
constexpr uint32_t streams = 16u;
constexpr uint32_t max_render_targets = 4u;
constexpr uint32_t clear_flags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
constexpr uint32_t max_clear_rects = 64u;
constexpr uint32_t max_lights = 1024u;
constexpr uint32_t clip_planes = 6u;
constexpr uint32_t matrix_values = 16u;
constexpr uint32_t max_declaration = 64u;
constexpr uint32_t max_stream_offset = 0xffffu;
constexpr uint32_t max_decl_type = D3DDECLTYPE_UNUSED;
constexpr uint32_t max_decl_method = D3DDECLMETHOD_LOOKUPPRESAMPLED;
constexpr uint32_t max_decl_usage = D3DDECLUSAGE_SAMPLE;
constexpr uint32_t max_usage_index = 15u;
constexpr uint32_t shader_padding = 16u;
constexpr uint32_t end_token = 0x0000ffffu;

bool valid_sampler(uint32_t sampler)
{
    AV_SCOPE("valid_sampler");
    return sampler < pixel_samplers || (sampler >= first_vertex_sampler && sampler <= last_vertex_sampler);
}

bool valid_transform(uint32_t state)
{
    AV_SCOPE("valid_transform");
    return state == D3DTS_VIEW || state == D3DTS_PROJECTION || (state >= D3DTS_TEXTURE0 && state <= D3DTS_TEXTURE7) ||
           (state >= D3DTS_WORLD && state <= D3DTS_WORLDMATRIX(255));
}

}

const uint32_t *D3d9Session::padded(const uint8_t *code, uint64_t code_size)
{
    AV_SCOPE("D3d9Session::padded");
    const uint64_t dwords = code_size / sizeof(uint32_t);
    uint32_t *const copy = arena_ != nullptr ? arena_->allocate<uint32_t>(dwords + shader_padding) : nullptr;
    if (copy == nullptr) return nullptr;
    memcpy(copy, code, code_size);
    for (uint32_t index = 0u; index < shader_padding; index++) copy[dwords + index] = end_token;
    return copy;
}

bool D3d9Session::on_CreateVertexShader9(uint32_t shader, const uint8_t *code, uint64_t code_size)
{
    AV_SCOPE("D3d9Session::on_CreateVertexShader9");
    const D3d9ShaderTokens tokens(code, code_size);
    if (device() == nullptr || !tokens.valid() || !tokens.vertex()) return false;
    const uint32_t *const copy = padded(code, code_size);
    if (copy == nullptr) return false;
    IDirect3DVertexShader9 *made = nullptr;
    return SUCCEEDED(device()->CreateVertexShader(reinterpret_cast<const DWORD *>(copy), &made)) &&
           keep(shader, D3dObjectKind::vertex_shader9, made);
}

bool D3d9Session::on_CreatePixelShader9(uint32_t shader, const uint8_t *code, uint64_t code_size)
{
    AV_SCOPE("D3d9Session::on_CreatePixelShader9");
    const D3d9ShaderTokens tokens(code, code_size);
    if (device() == nullptr || !tokens.valid() || tokens.vertex()) return false;
    const uint32_t *const copy = padded(code, code_size);
    if (copy == nullptr) return false;
    IDirect3DPixelShader9 *made = nullptr;
    return SUCCEEDED(device()->CreatePixelShader(reinterpret_cast<const DWORD *>(copy), &made)) &&
           keep(shader, D3dObjectKind::pixel_shader9, made);
}

bool D3d9Session::on_CreateVertexDeclaration9(uint32_t declaration, uint32_t count,
                                              const d3d9proto::VertexElement *elements)
{
    AV_SCOPE("D3d9Session::on_CreateVertexDeclaration9");
    if (device() == nullptr || count == 0u || count > max_declaration) return false;
    D3DVERTEXELEMENT9 list[max_declaration + 1u];
    for (uint32_t index = 0u; index < count; index++) {
        const d3d9proto::VertexElement &element = elements[index];
        if (element.stream >= streams || element.offset > max_stream_offset || element.type > max_decl_type ||
            element.method > max_decl_method || element.usage > max_decl_usage ||
            element.usage_index > max_usage_index)
            return false;
        list[index] = D3DVERTEXELEMENT9{static_cast<WORD>(element.stream), static_cast<WORD>(element.offset),
                                        static_cast<BYTE>(element.type), static_cast<BYTE>(element.method),
                                        static_cast<BYTE>(element.usage), static_cast<BYTE>(element.usage_index)};
    }
    list[count] = D3DVERTEXELEMENT9{0xff, 0u, D3DDECLTYPE_UNUSED, 0u, 0u, 0u};
    IDirect3DVertexDeclaration9 *made = nullptr;
    return SUCCEEDED(device()->CreateVertexDeclaration(list, &made)) &&
           keep(declaration, D3dObjectKind::vertex_declaration9, made);
}

bool D3d9Session::on_SetVertexShader9(uint32_t shader)
{
    AV_SCOPE("D3d9Session::on_SetVertexShader9");
    auto *const bound = mine<IDirect3DVertexShader9>(shader, D3dObjectKind::vertex_shader9);
    if (device() == nullptr || (shader != 0u && bound == nullptr)) return false;
    return SUCCEEDED(device()->SetVertexShader(bound));
}

bool D3d9Session::on_SetPixelShader9(uint32_t shader)
{
    AV_SCOPE("D3d9Session::on_SetPixelShader9");
    auto *const bound = mine<IDirect3DPixelShader9>(shader, D3dObjectKind::pixel_shader9);
    if (device() == nullptr || (shader != 0u && bound == nullptr)) return false;
    return SUCCEEDED(device()->SetPixelShader(bound));
}

bool D3d9Session::on_SetVertexDeclaration9(uint32_t declaration)
{
    AV_SCOPE("D3d9Session::on_SetVertexDeclaration9");
    auto *const bound = mine<IDirect3DVertexDeclaration9>(declaration, D3dObjectKind::vertex_declaration9);
    if (device() == nullptr || (declaration != 0u && bound == nullptr)) return false;
    return SUCCEEDED(device()->SetVertexDeclaration(bound));
}

bool D3d9Session::on_SetFvf9(uint32_t fvf)
{
    AV_SCOPE("D3d9Session::on_SetFvf9");
    return device() != nullptr && SUCCEEDED(device()->SetFVF(fvf));
}

bool D3d9Session::on_SetConstantsF9(uint32_t stage, uint32_t start, uint32_t value_count, const float *values)
{
    AV_SCOPE("D3d9Session::on_SetConstantsF9");
    const uint32_t registers = value_count / components;
    const uint32_t limit = stage == stage_vertex ? vertex_float_registers : pixel_float_registers;
    if (device() == nullptr || stage > stage_pixel || value_count % components != 0u || start > limit ||
        registers > limit - start)
        return false;
    IDirect3DDevice9 *const host = device();
    return SUCCEEDED(stage == stage_vertex ? host->SetVertexShaderConstantF(start, values, registers)
                                           : host->SetPixelShaderConstantF(start, values, registers));
}

bool D3d9Session::on_SetConstantsI9(uint32_t stage, uint32_t start, uint32_t value_count, const int32_t *values)
{
    AV_SCOPE("D3d9Session::on_SetConstantsI9");
    const uint32_t registers = value_count / components;
    if (device() == nullptr || stage > stage_pixel || value_count % components != 0u || start > integer_registers ||
        registers > integer_registers - start)
        return false;
    IDirect3DDevice9 *const host = device();
    return SUCCEEDED(stage == stage_vertex ? host->SetVertexShaderConstantI(start, values, registers)
                                           : host->SetPixelShaderConstantI(start, values, registers));
}

bool D3d9Session::on_SetConstantsB9(uint32_t stage, uint32_t start, uint32_t value_count, const uint32_t *values)
{
    AV_SCOPE("D3d9Session::on_SetConstantsB9");
    if (device() == nullptr || stage > stage_pixel || start > boolean_registers ||
        value_count > boolean_registers - start)
        return false;
    BOOL booleans[boolean_registers] = {};
    for (uint32_t index = 0u; index < value_count; index++) booleans[index] = values[index] != 0u ? TRUE : FALSE;
    IDirect3DDevice9 *const host = device();
    return SUCCEEDED(stage == stage_vertex ? host->SetVertexShaderConstantB(start, booleans, value_count)
                                           : host->SetPixelShaderConstantB(start, booleans, value_count));
}

bool D3d9Session::on_SetRenderStates9(uint32_t value_count, const uint32_t *values)
{
    AV_SCOPE("D3d9Session::on_SetRenderStates9");
    if (device() == nullptr || value_count % pair != 0u) return false;
    for (uint32_t index = 0u; index < value_count; index += pair)
        if (values[index] > max_render_state) return false;
    for (uint32_t index = 0u; index < value_count; index += pair)
        device()->SetRenderState(static_cast<D3DRENDERSTATETYPE>(values[index]), values[index + 1u]);
    return true;
}

bool D3d9Session::on_SetSamplerStates9(uint32_t value_count, const uint32_t *values)
{
    AV_SCOPE("D3d9Session::on_SetSamplerStates9");
    if (device() == nullptr || value_count % triple != 0u) return false;
    for (uint32_t index = 0u; index < value_count; index += triple)
        if (!valid_sampler(values[index]) || values[index + 1u] == 0u || values[index + 1u] > max_sampler_state)
            return false;
    for (uint32_t index = 0u; index < value_count; index += triple)
        device()->SetSamplerState(values[index], static_cast<D3DSAMPLERSTATETYPE>(values[index + 1u]),
                                            values[index + 2u]);
    return true;
}

bool D3d9Session::on_SetTextureStageStates9(uint32_t value_count, const uint32_t *values)
{
    AV_SCOPE("D3d9Session::on_SetTextureStageStates9");
    if (device() == nullptr || value_count % triple != 0u) return false;
    for (uint32_t index = 0u; index < value_count; index += triple)
        if (values[index] >= texture_stages || values[index + 1u] == 0u || values[index + 1u] > max_stage_state)
            return false;
    for (uint32_t index = 0u; index < value_count; index += triple)
        device()->SetTextureStageState(
            values[index], static_cast<D3DTEXTURESTAGESTATETYPE>(values[index + 1u]), values[index + 2u]);
    return true;
}

bool D3d9Session::on_SetTexture9(uint32_t sampler, uint32_t texture)
{
    AV_SCOPE("D3d9Session::on_SetTexture9");
    if (device() == nullptr || !valid_sampler(sampler)) return false;
    IDirect3DBaseTexture9 *bound = mine<IDirect3DTexture9>(texture, D3dObjectKind::texture9);
    if (bound == nullptr) bound = mine<IDirect3DCubeTexture9>(texture, D3dObjectKind::cube_texture9);
    if (bound == nullptr) bound = mine<IDirect3DVolumeTexture9>(texture, D3dObjectKind::volume_texture9);
    if (texture != 0u && bound == nullptr) return false;
    return SUCCEEDED(device()->SetTexture(sampler, bound));
}

bool D3d9Session::on_SetStreamSource9(uint32_t stream, uint32_t buffer, uint32_t offset, uint32_t stride)
{
    AV_SCOPE("D3d9Session::on_SetStreamSource9");
    auto *const bound = mine<IDirect3DVertexBuffer9>(buffer, D3dObjectKind::vertex_buffer9);
    if (device() == nullptr || stream >= streams || (buffer != 0u && bound == nullptr)) return false;
    return SUCCEEDED(device()->SetStreamSource(stream, bound, offset, stride)) &&
           slices_.stream_bound(device(), stream, buffer);
}

bool D3d9Session::on_SetStreamSourceFreq9(uint32_t stream, uint32_t setting)
{
    AV_SCOPE("D3d9Session::on_SetStreamSourceFreq9");
    return device() != nullptr && stream < streams &&
           SUCCEEDED(device()->SetStreamSourceFreq(stream, setting));
}

bool D3d9Session::on_SetIndices9(uint32_t buffer)
{
    AV_SCOPE("D3d9Session::on_SetIndices9");
    auto *const bound = mine<IDirect3DIndexBuffer9>(buffer, D3dObjectKind::index_buffer9);
    if (device() == nullptr || (buffer != 0u && bound == nullptr)) return false;
    return SUCCEEDED(device()->SetIndices(bound)) && slices_.indices_bound(device(), buffer);
}

bool D3d9Session::on_SetViewport9(const d3d9proto::Viewport &viewport)
{
    AV_SCOPE("D3d9Session::on_SetViewport9");
    const D3DVIEWPORT9 set = {viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_z, viewport.max_z};
    return device() != nullptr && SUCCEEDED(device()->SetViewport(&set));
}

bool D3d9Session::on_SetScissorRect9(const d3d9proto::Rect &rect)
{
    AV_SCOPE("D3d9Session::on_SetScissorRect9");
    const RECT set = {rect.left, rect.top, rect.right, rect.bottom};
    return device() != nullptr && SUCCEEDED(device()->SetScissorRect(&set));
}

bool D3d9Session::on_SetTransform9(uint32_t state, uint32_t value_count, const float *matrix)
{
    AV_SCOPE("D3d9Session::on_SetTransform9");
    if (device() == nullptr || value_count != matrix_values || !valid_transform(state)) return false;
    D3DMATRIX set = {};
    memcpy(&set, matrix, sizeof(set));
    return SUCCEEDED(device()->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(state), &set));
}

bool D3d9Session::on_SetMaterial9(const d3d9proto::Material &material)
{
    AV_SCOPE("D3d9Session::on_SetMaterial9");
    static_assert(sizeof(D3DMATERIAL9) == sizeof(d3d9proto::Material), "the wire's material is D3DMATERIAL9's");
    D3DMATERIAL9 set = {};
    memcpy(&set, &material, sizeof(set));
    return device() != nullptr && SUCCEEDED(device()->SetMaterial(&set));
}

bool D3d9Session::on_SetLight9(uint32_t index, const d3d9proto::Light &light)
{
    AV_SCOPE("D3d9Session::on_SetLight9");
    static_assert(sizeof(D3DLIGHT9) == sizeof(d3d9proto::Light), "the wire's light is D3DLIGHT9's");
    if (device() == nullptr || index >= max_lights || light.type < D3DLIGHT_POINT || light.type > D3DLIGHT_DIRECTIONAL)
        return false;
    D3DLIGHT9 set = {};
    memcpy(&set, &light, sizeof(set));
    return SUCCEEDED(device()->SetLight(index, &set));
}

bool D3d9Session::on_LightEnable9(uint32_t index, uint32_t enable)
{
    AV_SCOPE("D3d9Session::on_LightEnable9");
    return device() != nullptr && index < max_lights &&
           SUCCEEDED(device()->LightEnable(index, enable != 0u ? TRUE : FALSE));
}

bool D3d9Session::on_SetClipPlane9(uint32_t index, uint32_t value_count, const float *plane)
{
    AV_SCOPE("D3d9Session::on_SetClipPlane9");
    return device() != nullptr && index < clip_planes && value_count == components &&
           SUCCEEDED(device()->SetClipPlane(index, plane));
}

bool D3d9Session::on_DrawPrimitive9(uint32_t type, uint32_t start_vertex, uint32_t primitive_count)
{
    AV_SCOPE("D3d9Session::on_DrawPrimitive9");
    lanes_.touch(device());
    if (device() == nullptr || type < D3DPT_POINTLIST || type > D3DPT_TRIANGLEFAN || primitive_count == 0u) return false;
    slices_.copy_in_use(device());
    return SUCCEEDED(
        device()->DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(type), start_vertex, primitive_count));
}

bool D3d9Session::on_DrawIndexedPrimitive9(uint32_t type, int32_t base_vertex, uint32_t min_index,
                                           uint32_t vertex_count, uint32_t start_index, uint32_t primitive_count)
{
    AV_SCOPE("D3d9Session::on_DrawIndexedPrimitive9");
    lanes_.touch(device());
    if (device() == nullptr || type < D3DPT_POINTLIST || type > D3DPT_TRIANGLEFAN || primitive_count == 0u) return false;
    slices_.copy_in_use(device());
    return SUCCEEDED(device()->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(type), base_vertex,
                                                              min_index, vertex_count, start_index,
                                                              primitive_count));
}

bool D3d9Session::on_CreateQuery9(uint32_t query, uint32_t type, uint32_t &hresult)
{
    AV_SCOPE("D3d9Session::on_CreateQuery9");
    hresult = static_cast<uint32_t>(D3DERR_INVALIDCALL);
    if (device() == nullptr) return false;
    IDirect3DQuery9 *made = nullptr;
    const HRESULT created = device()->CreateQuery(static_cast<D3DQUERYTYPE>(type), &made);
    if (FAILED(created)) {
        hresult = static_cast<uint32_t>(created);
        return true;
    }
    hresult = S_OK;
    return keep(query, D3dObjectKind::query9, made);
}

bool D3d9Session::on_IssueQuery9(uint32_t query, uint32_t flags)
{
    AV_SCOPE("D3d9Session::on_IssueQuery9");
    auto *const issued = mine<IDirect3DQuery9>(query, D3dObjectKind::query9);
    if (issued == nullptr || (flags != D3DISSUE_BEGIN && flags != D3DISSUE_END)) return false;
    return SUCCEEDED(issued->Issue(flags));
}

bool D3d9Session::on_GetQueryData9(uint32_t query, uint32_t flags, uint32_t &hresult, uint8_t *data,
                                   uint64_t data_capacity, uint64_t &data_size)
{
    AV_SCOPE("D3d9Session::on_GetQueryData9");
    data_size = 0u;
    hresult = static_cast<uint32_t>(D3DERR_INVALIDCALL);
    auto *const asked = mine<IDirect3DQuery9>(query, D3dObjectKind::query9);
    if (asked == nullptr) return true;
    const DWORD size = asked->GetDataSize();
    if (size > data_capacity) return true;
    const HRESULT answer = asked->GetData(data, size, flags & D3DGETDATA_FLUSH);
    hresult = static_cast<uint32_t>(answer);
    if (answer == S_OK) data_size = size;
    return true;
}

bool D3d9Session::on_SetRenderTarget9(uint32_t index, uint32_t surface_id, uint32_t subresource)
{
    AV_SCOPE("D3d9Session::on_SetRenderTarget9");
    if (device() == nullptr || index >= max_render_targets) return false;
    if (surface_id == 0u) return index != 0u && SUCCEEDED(device()->SetRenderTarget(index, nullptr));
    IDirect3DSurface9 *const target = surface(surface_id, subresource);
    if (target == nullptr) return false;
    ScopeExit release([target] { target->Release(); });
    return SUCCEEDED(device()->SetRenderTarget(index, target));
}

bool D3d9Session::on_SetDepthStencil9(uint32_t surface_id, uint32_t subresource)
{
    AV_SCOPE("D3d9Session::on_SetDepthStencil9");
    if (device() == nullptr) return false;
    if (surface_id == 0u) return SUCCEEDED(device()->SetDepthStencilSurface(nullptr));
    IDirect3DSurface9 *const target = surface(surface_id, subresource);
    if (target == nullptr) return false;
    ScopeExit release([target] { target->Release(); });
    return SUCCEEDED(device()->SetDepthStencilSurface(target));
}

bool D3d9Session::on_Clear9(uint32_t rect_count, const d3d9proto::Rect *rects, uint32_t flags, uint32_t color,
                            float z, uint32_t stencil)
{
    AV_SCOPE("D3d9Session::on_Clear9");
    lanes_.touch(device());
    if (device() == nullptr || flags == 0u || (flags & ~clear_flags) != 0u) return false;
    D3DRECT areas[max_clear_rects];
    for (uint32_t index = 0u; index < rect_count; index++)
        areas[index] = D3DRECT{rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    return SUCCEEDED(device()->Clear(rect_count, rect_count != 0u ? areas : nullptr, flags, color, z,
                                               stencil));
}

}
