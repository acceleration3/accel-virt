#include "framework.h"

namespace samples {

// ---------------------------------------------------------------- text

void copy_text(char *to, size_t capacity, const char *from)
{
    size_t i = 0;
    for (; i + 1 < capacity && from[i] != '\0'; i++) to[i] = from[i];
    if (capacity != 0) to[i] = '\0';
}

void append_text(char *to, size_t capacity, const char *from)
{
    size_t at = 0;
    while (at < capacity && to[at] != '\0') at++;
    if (at < capacity) copy_text(to + at, capacity - at, from);
}

void append_number(char *to, size_t capacity, double value, int decimals)
{
    char digits[48] = {};
    int  n = 0;
    if (value < 0) {
        digits[n++] = '-';
        value = -value;
    }
    double scale = 1.0;
    for (int d = 0; d < decimals; d++) scale *= 10.0;
    unsigned long long whole = static_cast<unsigned long long>(value * scale + 0.5);
    char reversed[32] = {};
    int  r = 0;
    for (int d = 0; d < decimals; d++) {
        reversed[r++] = static_cast<char>('0' + whole % 10u);
        whole /= 10u;
    }
    if (decimals > 0) reversed[r++] = '.';
    do {
        reversed[r++] = static_cast<char>('0' + whole % 10u);
        whole /= 10u;
    } while (whole != 0u && r < 30);
    while (r > 0) digits[n++] = reversed[--r];
    append_text(to, capacity, digits);
}

void copy_bytes(void *to, const void *from, size_t bytes)
{
    uint8_t       *d = static_cast<uint8_t *>(to);
    const uint8_t *f = static_cast<const uint8_t *>(from);
    for (size_t i = 0; i < bytes; i++) d[i] = f[i];
}

bool same_text(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

// ---------------------------------------------------------------- meshes

void Mesh::release()
{
    release_memory(vertices);
    release_memory(indices);
    vertices = nullptr;
    indices = nullptr;
}

namespace {

Mesh allocate_mesh(uint32_t vertices, uint32_t indices)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
    return mesh;
}

void push_vertex(Mesh &mesh, float3 p, float3 n, float2 uv, float3 color)
{
    mesh.vertices[mesh.vertex_count++] = Vertex{p, n, uv, color};
}

void push_quad(Mesh &mesh, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    // Front faces are clockwise as seen: every generator winds its quads to
    // face the way its normals point, whatever order it listed them in.
    const Vertex *v = mesh.vertices;
    const float3 facing = cross(v[b].position - v[a].position, v[c].position - v[a].position);
    if (dot(facing, v[a].normal + v[b].normal + v[c].normal + v[d].normal) < 0) {
        const uint32_t swap = b;
        b = d;
        d = swap;
    }
    uint32_t *i = mesh.indices + mesh.index_count;
    i[0] = a; i[1] = b; i[2] = c;
    i[3] = a; i[4] = c; i[5] = d;
    mesh.index_count += 6;
}

}  // namespace

Mesh cube_mesh(float size)
{
    Mesh mesh = allocate_mesh(24, 36);
    const float h = size * 0.5f;
    const float3 normals[6] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const float3 colors[6] = {{1, 0.3f, 0.3f}, {0.3f, 1, 0.3f}, {0.3f, 0.3f, 1}, {1, 1, 0.3f}, {0.3f, 1, 1}, {1, 0.3f, 1}};
    for (int f = 0; f < 6; f++) {
        const float3 n = normals[f];
        const float3 u = abs(n.y) > 0.5f ? float3{1, 0, 0} : normalize(cross({0, 1, 0}, n));
        const float3 v = cross(n, u);
        const uint32_t base = mesh.vertex_count;
        const float2 uvs[4] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
        const float su[4] = {-1, -1, 1, 1}, sv[4] = {-1, 1, 1, -1};
        for (int k = 0; k < 4; k++)
            push_vertex(mesh, (n + u * su[k] + v * sv[k]) * h, n, uvs[k], colors[f]);
        push_quad(mesh, base, base + 1, base + 2, base + 3);
    }
    return mesh;
}

Mesh sphere_mesh(float radius, uint32_t slices, uint32_t stacks)
{
    Mesh mesh = allocate_mesh((slices + 1) * (stacks + 1), slices * stacks * 6);
    for (uint32_t j = 0; j <= stacks; j++) {
        const float v = static_cast<float>(j) / static_cast<float>(stacks);
        const float phi = v * pi;
        for (uint32_t i = 0; i <= slices; i++) {
            const float u = static_cast<float>(i) / static_cast<float>(slices);
            const float theta = u * 2.0f * pi;
            const float3 n = {sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta)};
            push_vertex(mesh, n * radius, n, {u, v}, {0.5f + 0.5f * n.x, 0.5f + 0.5f * n.y, 0.5f + 0.5f * n.z});
        }
    }
    for (uint32_t j = 0; j < stacks; j++)
        for (uint32_t i = 0; i < slices; i++) {
            const uint32_t a = j * (slices + 1) + i, b = a + slices + 1;
            push_quad(mesh, a, a + 1, b + 1, b);
        }
    return mesh;
}

Mesh torus_mesh(float radius, float tube, uint32_t rings, uint32_t sides)
{
    Mesh mesh = allocate_mesh((rings + 1) * (sides + 1), rings * sides * 6);
    for (uint32_t r = 0; r <= rings; r++) {
        const float u = static_cast<float>(r) / static_cast<float>(rings), a = u * 2.0f * pi;
        const float3 center = {cos(a) * radius, 0, sin(a) * radius};
        for (uint32_t s = 0; s <= sides; s++) {
            const float v = static_cast<float>(s) / static_cast<float>(sides), b = v * 2.0f * pi;
            const float3 n = {cos(a) * cos(b), sin(b), sin(a) * cos(b)};
            push_vertex(mesh, center + n * tube, n, {u * 4.0f, v}, {0.8f, 0.6f + 0.4f * v, 0.3f});
        }
    }
    for (uint32_t r = 0; r < rings; r++)
        for (uint32_t s = 0; s < sides; s++) {
            const uint32_t a = r * (sides + 1) + s, b = a + sides + 1;
            push_quad(mesh, a, b, b + 1, a + 1);
        }
    return mesh;
}

Mesh plane_mesh(float size, uint32_t cells, float uv_scale)
{
    Mesh mesh = allocate_mesh((cells + 1) * (cells + 1), cells * cells * 6);
    for (uint32_t j = 0; j <= cells; j++)
        for (uint32_t i = 0; i <= cells; i++) {
            const float u = static_cast<float>(i) / static_cast<float>(cells);
            const float v = static_cast<float>(j) / static_cast<float>(cells);
            push_vertex(mesh, {(u - 0.5f) * size, 0, (v - 0.5f) * size}, {0, 1, 0}, {u * uv_scale, v * uv_scale},
                        {0.7f, 0.7f, 0.7f});
        }
    for (uint32_t j = 0; j < cells; j++)
        for (uint32_t i = 0; i < cells; i++) {
            const uint32_t a = j * (cells + 1) + i, b = a + cells + 1;
            push_quad(mesh, a, b, b + 1, a + 1);
        }
    return mesh;
}

// The gears sample's gear: a toothed ring with front, back, outer and inner
// faces, flat shaded, as the old OpenGL gears demo built it.
Mesh gear_mesh(float inner, float outer, float width, uint32_t teeth, float tooth_depth, float3 color)
{
    Mesh mesh = allocate_mesh(teeth * 40, teeth * 60);
    const float r0 = inner, r1 = outer - tooth_depth * 0.5f, r2 = outer + tooth_depth * 0.5f;
    const float da = 2.0f * pi / static_cast<float>(teeth) / 4.0f;
    const float hw = width * 0.5f;
    auto quad = [&](float3 a, float3 b, float3 c, float3 d, float3 n) {
        // Front faces are clockwise as seen: wind every quad to face along its normal.
        if (dot(cross(b - a, c - a), n) < 0) {
            const float3 swap = b;
            b = d;
            d = swap;
        }
        const uint32_t base = mesh.vertex_count;
        push_vertex(mesh, a, n, {0, 0}, color);
        push_vertex(mesh, b, n, {1, 0}, color);
        push_vertex(mesh, c, n, {1, 1}, color);
        push_vertex(mesh, d, n, {0, 1}, color);
        push_quad(mesh, base, base + 1, base + 2, base + 3);
    };
    for (uint32_t i = 0; i < teeth; i++) {
        const float a = static_cast<float>(i) * 2.0f * pi / static_cast<float>(teeth);
        const float c0 = cos(a), s0 = sin(a), c1 = cos(a + da), s1 = sin(a + da);
        const float c2 = cos(a + 2 * da), s2 = sin(a + 2 * da), c3 = cos(a + 3 * da), s3 = sin(a + 3 * da);
        const float c4 = cos(a + 4 * da), s4 = sin(a + 4 * da);
        for (int side = 0; side < 2; side++) {
            const float z = side == 0 ? hw : -hw;
            const float3 n = {0, 0, side == 0 ? 1.0f : -1.0f};
            // ring face between the teeth's root and the hole
            quad({r0 * c0, r0 * s0, z}, {r1 * c0, r1 * s0, z}, {r1 * c4, r1 * s4, z}, {r0 * c4, r0 * s4, z}, n);
            // the tooth's face
            quad({r1 * c0, r1 * s0, z}, {r2 * c1, r2 * s1, z}, {r2 * c2, r2 * s2, z}, {r1 * c3, r1 * s3, z}, n);
        }
        // outward faces: rising flank, top, falling flank, root
        const float3 p[5] = {{r1 * c0, r1 * s0, 0}, {r2 * c1, r2 * s1, 0}, {r2 * c2, r2 * s2, 0},
                             {r1 * c3, r1 * s3, 0}, {r1 * c4, r1 * s4, 0}};
        for (int k = 0; k < 4; k++) {
            const float3 e = p[k + 1] - p[k];
            const float3 n = normalize({e.y, -e.x, 0});
            quad({p[k].x, p[k].y, hw}, {p[k].x, p[k].y, -hw}, {p[k + 1].x, p[k + 1].y, -hw},
                 {p[k + 1].x, p[k + 1].y, hw}, n);
        }
        // the hole
        quad({r0 * c4, r0 * s4, hw}, {r0 * c4, r0 * s4, -hw}, {r0 * c0, r0 * s0, -hw}, {r0 * c0, r0 * s0, hw},
             {-c0, -s0, 0});
    }
    return mesh;
}


// ---------------------------------------------------------------- GPU helpers

namespace {

constexpr uint32_t static_views = 32768;     // a sample's views, for its lifetime
constexpr uint32_t views_per_slot = 8192;     // a frame's transient views
constexpr uint64_t ring_bytes = 32ull << 20;  // a frame's upload memory
constexpr uint32_t shader_cache = 512;

uint64_t align_up(uint64_t value, uint64_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

D3D12_HEAP_PROPERTIES heap_of(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = type;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    return heap;
}

D3D12_RESOURCE_DESC buffer_desc(uint64_t bytes, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    return desc;
}

ID3D12Resource *upload_buffer(ID3D12Device *device, uint64_t bytes)
{
    const D3D12_HEAP_PROPERTIES heap = heap_of(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC   desc = buffer_desc(bytes);
    ID3D12Resource *made = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                               nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void **>(&made))))
        return nullptr;
    return made;
}

// One mip level down, each texel the mean of the four (or two, or one) above it.
void downsample(const uint32_t *from, uint32_t w, uint32_t h, uint32_t *to)
{
    const uint32_t nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
    for (uint32_t y = 0; y < nh; y++)
        for (uint32_t x = 0; x < nw; x++) {
            uint32_t sum[4] = {};
            uint32_t n = 0;
            for (uint32_t dy = 0; dy < (h > 1 ? 2u : 1u); dy++)
                for (uint32_t dx = 0; dx < (w > 1 ? 2u : 1u); dx++) {
                    const uint32_t t = from[(y * (h > 1 ? 2 : 1) + dy) * w + x * (w > 1 ? 2 : 1) + dx];
                    for (uint32_t c = 0; c < 4; c++) sum[c] += (t >> (c * 8u)) & 0xFFu;
                    n++;
                }
            uint32_t texel = 0;
            for (uint32_t c = 0; c < 4; c++) texel |= ((sum[c] + n / 2) / n) << (c * 8u);
            to[y * nw + x] = texel;
        }
}

}  // namespace

D3D12_CPU_DESCRIPTOR_HANDLE Descriptors::cpu_at(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE at = cpu;
    at.ptr += static_cast<SIZE_T>(index) * stride;
    return at;
}

D3D12_GPU_DESCRIPTOR_HANDLE Descriptors::gpu_at(uint32_t index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE at = gpu;
    at.ptr += static_cast<UINT64>(index) * stride;
    return at;
}

bool DescriptorHeap::make(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool visible)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = count;
    desc.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void **>(&heap))))
        return false;
    stride = device->GetDescriptorHandleIncrementSize(type);
    capacity = count;
    used = 0;
    shader_visible = visible;
    return true;
}

Descriptors DescriptorHeap::take(uint32_t count)
{
    Descriptors run;
    if (heap == nullptr || used + count > capacity) return run;
    run.stride = stride;
    run.count = count;
    run.cpu = heap->GetCPUDescriptorHandleForHeapStart();
    run.cpu.ptr += static_cast<SIZE_T>(used) * stride;
    if (shader_visible) {
        run.gpu = heap->GetGPUDescriptorHandleForHeapStart();
        run.gpu.ptr += static_cast<UINT64>(used) * stride;
    }
    used += count;
    return run;
}

void DescriptorHeap::release() { safe_release(heap); }

void DrawMesh::release()
{
    safe_release(vertices);
    safe_release(indices);
}

void DrawMesh::draw(ID3D12GraphicsCommandList *list, uint32_t instances) const
{
    list->IASetVertexBuffers(0, 1, &vertex_view);
    list->IASetIndexBuffer(&index_view);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawIndexedInstanced(index_count, instances, 0, 0, 0);
}

void Texture::release() { safe_release(resource); }

// ---- root signatures

RootLayout &RootLayout::descriptor(D3D12_ROOT_PARAMETER_TYPE type, uint32_t reg, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_ROOT_PARAMETER &p = params[param_count++];
    p.ParameterType = type;
    p.Descriptor.ShaderRegister = reg;
    p.Descriptor.RegisterSpace = 0;
    p.ShaderVisibility = visibility;
    return *this;
}

RootLayout &RootLayout::cbv(uint32_t reg, D3D12_SHADER_VISIBILITY visibility)
{
    return descriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, reg, visibility);
}

RootLayout &RootLayout::srv(uint32_t reg, D3D12_SHADER_VISIBILITY visibility)
{
    return descriptor(D3D12_ROOT_PARAMETER_TYPE_SRV, reg, visibility);
}

RootLayout &RootLayout::uav(uint32_t reg, D3D12_SHADER_VISIBILITY visibility)
{
    return descriptor(D3D12_ROOT_PARAMETER_TYPE_UAV, reg, visibility);
}

RootLayout &RootLayout::constants(uint32_t reg, uint32_t values, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_ROOT_PARAMETER &p = params[param_count++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p.Constants.ShaderRegister = reg;
    p.Constants.RegisterSpace = 0;
    p.Constants.Num32BitValues = values;
    p.ShaderVisibility = visibility;
    return *this;
}

// The range's index rides in the pointer until root_signature() resolves it:
// a layout may be copied, and a pointer into it would not follow.
RootLayout &RootLayout::table(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t reg, uint32_t count,
                              D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_DESCRIPTOR_RANGE &r = ranges[range_count];
    r.RangeType = type;
    r.NumDescriptors = count;
    r.BaseShaderRegister = reg;
    r.RegisterSpace = 0;
    r.OffsetInDescriptorsFromTableStart = 0;
    D3D12_ROOT_PARAMETER &p = params[param_count++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p.DescriptorTable.NumDescriptorRanges = 1;
    p.DescriptorTable.pDescriptorRanges = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE *>(static_cast<uintptr_t>(range_count));
    p.ShaderVisibility = visibility;
    range_count++;
    return *this;
}

RootLayout &RootLayout::srv_table(uint32_t reg, uint32_t count, D3D12_SHADER_VISIBILITY visibility)
{
    return table(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, reg, count, visibility);
}

RootLayout &RootLayout::uav_table(uint32_t reg, uint32_t count, D3D12_SHADER_VISIBILITY visibility)
{
    return table(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, reg, count, visibility);
}

RootLayout &RootLayout::sampler_table(uint32_t reg, uint32_t count, D3D12_SHADER_VISIBILITY visibility)
{
    return table(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, reg, count, visibility);
}

RootLayout &RootLayout::sampler(uint32_t reg, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address,
                                D3D12_SHADER_VISIBILITY visibility, D3D12_COMPARISON_FUNC comparison)
{
    D3D12_STATIC_SAMPLER_DESC &s = samplers[sampler_count++];
    s.Filter = filter;
    s.AddressU = s.AddressV = s.AddressW = address;
    s.MipLODBias = 0;
    s.MaxAnisotropy = 8;
    s.ComparisonFunc = comparison;
    s.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    s.MinLOD = 0;
    s.MaxLOD = D3D12_FLOAT32_MAX;
    s.ShaderRegister = reg;
    s.RegisterSpace = 0;
    s.ShaderVisibility = visibility;
    return *this;
}

RootLayout &RootLayout::no_input()
{
    input = false;
    return *this;
}

// ---- the device's lifetime

const D3D12_INPUT_ELEMENT_DESC Gpu::mesh_layout[4] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

namespace {

/* A step of setting the device up that failed, named with its HRESULT: "no
 * Direct3D 12 device" alone does not say which of a dozen it was. */
bool made(HRESULT result, const char *what)
{
    if (SUCCEEDED(result)) return true;
    char line[128] = {};
    append_text(line, sizeof line, "FAIL: ");
    append_text(line, sizeof line, what);
    append_text(line, sizeof line, " 0x");
    char hex[9] = {};
    for (int digit = 0; digit < 8; digit++)
        hex[digit] = "0123456789abcdef"[(static_cast<unsigned>(result) >> (28 - 4 * digit)) & 0xfu];
    append_text(line, sizeof line, hex);
    append_text(line, sizeof line, "\n");
    log(line);
    return false;
}

}  // namespace

bool Gpu::initialize()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (!made(device->CreateCommandQueue(&queue_desc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void **>(&queue)),
              "CreateCommandQueue"))
        return false;
    if (!made(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void **>(&fence_)),
              "CreateFence"))
        return false;
    for (Slot &s : slots_) {
        if (!made(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                 reinterpret_cast<void **>(&s.allocator)),
                  "CreateCommandAllocator"))
            return false;
        s.ring = upload_buffer(device, ring_bytes);
        if (s.ring == nullptr) return made(E_FAIL, "upload ring");
        const D3D12_RANGE none = {0, 0};
        if (!made(s.ring->Map(0, &none, reinterpret_cast<void **>(&s.ring_cpu)), "Map of the upload ring")) return false;
        s.ring_gpu = s.ring->GetGPUVirtualAddress();
    }
    if (!made(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, slots_[0].allocator, nullptr,
                                        __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void **>(&list)),
              "CreateCommandList"))
        return false;
    list->Close();
    if (!views_.make(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, static_views + slots * views_per_slot, true) ||
        !samplers_.make(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, true) ||
        !cpu_views_.make(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, false) ||
        !rtvs_.make(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 512, false) ||
        !dsvs_.make(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128, false))
        return made(E_FAIL, "a descriptor heap");
    views_.capacity = static_views;
    shaders_ = static_cast<CachedShader *>(allocate(sizeof(CachedShader) * shader_cache));

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = color_format;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    color = resource(desc, D3D12_RESOURCE_STATE_RENDER_TARGET);
    desc.Format = depth_format;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = depth_format;
    clear.DepthStencil.Depth = 1.0f;
    depth_buffer = resource(desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear);
    if (color == nullptr || depth_buffer == nullptr) return made(E_FAIL, "the render target or depth buffer");
    target = rtvs_.take(1).cpu;
    depth = dsvs_.take(1).cpu;
    device->CreateRenderTargetView(color, nullptr, target);
    device->CreateDepthStencilView(depth_buffer, nullptr, depth);
    return !failed;
}

void Gpu::shutdown()
{
    finish();
    for (uint32_t i = 0; i < shader_count_; i++) release_memory(const_cast<void *>(shaders_[i].code.pShaderBytecode));
    release_memory(shaders_);
    shaders_ = nullptr;
    shader_count_ = 0;
    safe_release(color);
    safe_release(depth_buffer);
    views_.release();
    samplers_.release();
    cpu_views_.release();
    rtvs_.release();
    dsvs_.release();
    safe_release(list);
    for (Slot &s : slots_) {
        safe_release(s.ring);
        safe_release(s.allocator);
    }
    safe_release(fence_);
    safe_release(queue);
}

void Gpu::open()
{
    slot_ = (slot_ + 1) % slots;
    Slot &s = slots_[slot_];
    wait(s.fence);
    if (FAILED(s.allocator->Reset()) || FAILED(list->Reset(s.allocator, nullptr))) {
        log("FAIL: the command list would not reset\n");
        failed = true;
        return;
    }
    s.ring_used = 0;
    frame_views_used_ = 0;
    ID3D12DescriptorHeap *const heaps[2] = {views_.heap, samplers_.heap};
    list->SetDescriptorHeaps(2, heaps);
    recording_ = true;
}

void Gpu::submit()
{
    if (!recording_) return;
    recording_ = false;
    if (FAILED(list->Close())) {
        log("FAIL: the command list would not close\n");
        failed = true;
        queued_count_ = 0;
        return;
    }
    ID3D12CommandList *lists[1 + 16] = {list};
    for (uint32_t i = 0; i < queued_count_; i++) lists[1 + i] = queued_[i];
    queue->ExecuteCommandLists(1 + queued_count_, lists);
    queued_count_ = 0;
    queue->Signal(fence_, ++fence_value_);
    slots_[slot_].fence = fence_value_;
}

bool Gpu::reached(uint64_t value) const { return fence_->GetCompletedValue() >= value; }

void Gpu::wait(uint64_t value)
{
    if (value == 0 || reached(value)) return;
    // A null event: the call returns once the fence reaches the value.
    if (FAILED(fence_->SetEventOnCompletion(value, nullptr))) {
        while (!reached(value)) {
        }
    }
}

void Gpu::finish()
{
    wait(fence_value_);
    for (uint32_t i = 0; i < kept_count_; i++) safe_release(kept_[i]);
    kept_count_ = 0;
}

void Gpu::reset_sample_state()
{
    views_.used = 0;
    samplers_.used = 0;
    cpu_views_.used = 0;
    rtvs_.used = 1;   // the frame's target
    dsvs_.used = 1;
}

void Gpu::queue_lists(ID3D12CommandList *const *lists, uint32_t count)
{
    for (uint32_t i = 0; i < count && queued_count_ < 16; i++) queued_[queued_count_++] = lists[i];
}

// ---- shaders and pipelines

D3D12_SHADER_BYTECODE Gpu::shader_code(const char *name, const char *source, const char *entry, const char *stage)
{
    for (uint32_t i = 0; i < shader_count_; i++)
        if (same_text(shaders_[i].name, name)) return shaders_[i].code;
    Blob code;
    if (!shader(name, source, entry, stage, code)) {
        failed = true;
        return {nullptr, 0};
    }
    if (shader_count_ == shader_cache) {
        failed = true;
        release_memory(code.data);
        return {nullptr, 0};
    }
    CachedShader &cached = shaders_[shader_count_++];
    copy_text(cached.name, sizeof cached.name, name);
    cached.code = {code.data, code.size};
    return cached.code;
}

D3D12_SHADER_BYTECODE Gpu::vs(const char *n, const char *s, const char *e) { return shader_code(n, s, e, "vs_6_0"); }
D3D12_SHADER_BYTECODE Gpu::ps(const char *n, const char *s, const char *e) { return shader_code(n, s, e, "ps_6_0"); }
D3D12_SHADER_BYTECODE Gpu::gs(const char *n, const char *s, const char *e) { return shader_code(n, s, e, "gs_6_0"); }
D3D12_SHADER_BYTECODE Gpu::hs(const char *n, const char *s, const char *e) { return shader_code(n, s, e, "hs_6_0"); }
D3D12_SHADER_BYTECODE Gpu::ds(const char *n, const char *s, const char *e) { return shader_code(n, s, e, "ds_6_0"); }
D3D12_SHADER_BYTECODE Gpu::cs(const char *n, const char *s, const char *e) { return shader_code(n, s, e, "cs_6_0"); }

ID3D12RootSignature *Gpu::root_signature(const RootLayout &layout)
{
    D3D12_ROOT_PARAMETER params[16] = {};
    for (uint32_t i = 0; i < layout.param_count; i++) {
        params[i] = layout.params[i];
        if (params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            params[i].DescriptorTable.pDescriptorRanges =
                &layout.ranges[reinterpret_cast<uintptr_t>(layout.params[i].DescriptorTable.pDescriptorRanges)];
    }
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = layout.param_count;
    desc.pParameters = params;
    desc.NumStaticSamplers = layout.sampler_count;
    desc.pStaticSamplers = layout.samplers;
    desc.Flags = layout.input ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;
    Blob blob;
    ID3D12RootSignature *made = nullptr;
    if (!serialize_root_signature(desc, blob) ||
        FAILED(device->CreateRootSignature(0, blob.data, blob.size, __uuidof(ID3D12RootSignature),
                                           reinterpret_cast<void **>(&made)))) {
        log("FAIL: a root signature\n");
        failed = true;
    }
    release_memory(blob.data);
    return made;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC Gpu::graphics(ID3D12RootSignature *root, D3D12_SHADER_BYTECODE vs_code,
                                                 D3D12_SHADER_BYTECODE ps_code)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = root;
    desc.VS = vs_code;
    desc.PS = ps_code;
    for (D3D12_RENDER_TARGET_BLEND_DESC &blend : desc.BlendState.RenderTarget) {
        blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    desc.SampleMask = 0xFFFFFFFFu;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.DepthStencilState.StencilReadMask = 0xFF;
    desc.DepthStencilState.StencilWriteMask = 0xFF;
    const D3D12_DEPTH_STENCILOP_DESC keep = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
                                             D3D12_COMPARISON_FUNC_ALWAYS};
    desc.DepthStencilState.FrontFace = keep;
    desc.DepthStencilState.BackFace = keep;
    desc.InputLayout = {mesh_layout, 4};
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = color_format;
    desc.DSVFormat = depth_format;
    desc.SampleDesc.Count = 1;
    return desc;
}

ID3D12PipelineState *Gpu::pipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc)
{
    ID3D12PipelineState *made = nullptr;
    if (desc.VS.pShaderBytecode == nullptr || desc.pRootSignature == nullptr ||
        FAILED(device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void **>(&made)))) {
        log("FAIL: a graphics pipeline\n");
        failed = true;
        return nullptr;
    }
    return made;
}

ID3D12PipelineState *Gpu::compute(ID3D12RootSignature *root, D3D12_SHADER_BYTECODE cs_code)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = root;
    desc.CS = cs_code;
    ID3D12PipelineState *made = nullptr;
    if (cs_code.pShaderBytecode == nullptr || root == nullptr ||
        FAILED(device->CreateComputePipelineState(&desc, __uuidof(ID3D12PipelineState),
                                                  reinterpret_cast<void **>(&made)))) {
        log("FAIL: a compute pipeline\n");
        failed = true;
        return nullptr;
    }
    return made;
}

// ---- resources

ID3D12Resource *Gpu::resource(const D3D12_RESOURCE_DESC &desc, D3D12_RESOURCE_STATES state,
                              const D3D12_CLEAR_VALUE *clear)
{
    const D3D12_HEAP_PROPERTIES heap = heap_of(D3D12_HEAP_TYPE_DEFAULT);
    ID3D12Resource *made = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, clear,
                                               __uuidof(ID3D12Resource), reinterpret_cast<void **>(&made)))) {
        log("FAIL: a resource\n");
        failed = true;
        return nullptr;
    }
    return made;
}

void Gpu::keep(ID3D12Resource *kept)
{
    if (kept_count_ == sizeof kept_ / sizeof kept_[0]) {
        failed = true;
        kept->Release();
        return;
    }
    kept_[kept_count_++] = kept;
}

ID3D12Resource *Gpu::buffer(const void *data, uint32_t bytes, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags)
{
    ID3D12Resource *made = resource(buffer_desc(bytes, flags), data != nullptr ? D3D12_RESOURCE_STATE_COPY_DEST : state);
    if (made == nullptr || data == nullptr) return made;
    ID3D12Resource *staging = upload_buffer(device, bytes);
    void *mapped = nullptr;
    if (staging == nullptr || FAILED(staging->Map(0, nullptr, &mapped))) {
        failed = true;
        safe_release(staging);
        return made;
    }
    copy_bytes(mapped, data, bytes);
    staging->Unmap(0, nullptr);
    list->CopyBufferRegion(made, 0, staging, 0, bytes);
    keep(staging);
    barrier(made, D3D12_RESOURCE_STATE_COPY_DEST, state);
    return made;
}

DrawMesh Gpu::mesh(const Mesh &source)
{
    DrawMesh drawn;
    const uint32_t vertex_bytes = source.vertex_count * static_cast<uint32_t>(sizeof(Vertex));
    drawn.vertices = buffer(source.vertices, vertex_bytes, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    drawn.indices = buffer(source.indices, source.index_count * 4u, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    drawn.index_count = source.index_count;
    if (drawn.vertices == nullptr || drawn.indices == nullptr) return drawn;
    drawn.vertex_view = {drawn.vertices->GetGPUVirtualAddress(), vertex_bytes, sizeof(Vertex)};
    drawn.index_view = {drawn.indices->GetGPUVirtualAddress(), source.index_count * 4u, DXGI_FORMAT_R32_UINT};
    return drawn;
}

void Gpu::upload_texture(ID3D12Resource *target_resource, uint32_t subresource, const void *data, uint32_t row_bytes,
                         uint32_t rows, uint32_t slices)
{
    const D3D12_RESOURCE_DESC desc = target_resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT   row_count = 0;
    UINT64 row_size = 0, total = 0;
    device->GetCopyableFootprints(&desc, subresource, 1, 0, &footprint, &row_count, &row_size, &total);
    ID3D12Resource *staging = upload_buffer(device, total);
    uint8_t *mapped = nullptr;
    if (staging == nullptr || FAILED(staging->Map(0, nullptr, reinterpret_cast<void **>(&mapped)))) {
        failed = true;
        safe_release(staging);
        return;
    }
    const uint8_t *from = static_cast<const uint8_t *>(data);
    for (uint32_t z = 0; z < slices; z++)
        for (uint32_t y = 0; y < rows && y < row_count; y++)
            copy_bytes(mapped + (static_cast<uint64_t>(z) * row_count + y) * footprint.Footprint.RowPitch,
                       from + (static_cast<uint64_t>(z) * rows + y) * row_bytes, row_bytes);
    staging->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION to = {};
    to.pResource = target_resource;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.SubresourceIndex = subresource;
    D3D12_TEXTURE_COPY_LOCATION from_location = {};
    from_location.pResource = staging;
    from_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    from_location.PlacedFootprint = footprint;
    list->CopyTextureRegion(&to, 0, 0, 0, &from_location, nullptr);
    keep(staging);
}

Texture Gpu::texture(const uint32_t *texels, uint32_t w, uint32_t h, uint32_t layers, bool cube, DXGI_FORMAT format,
                     bool mips)
{
    Texture made;
    uint32_t levels = 1;
    if (mips)
        while ((w >> levels) > 0 || (h >> levels) > 0) levels++;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = w;
    desc.Height = h;
    desc.DepthOrArraySize = static_cast<UINT16>(layers);
    desc.MipLevels = static_cast<UINT16>(levels);
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    made.resource = resource(desc, D3D12_RESOURCE_STATE_COPY_DEST);
    if (made.resource == nullptr) return made;
    uint32_t *scratch = static_cast<uint32_t *>(allocate(static_cast<size_t>(w) * h * 4u));
    uint32_t *next = static_cast<uint32_t *>(allocate(static_cast<size_t>(w) * h * 4u));
    for (uint32_t layer = 0; layer < layers; layer++) {
        const uint32_t *level = texels + static_cast<size_t>(layer) * w * h;
        uint32_t lw = w, lh = h;
        for (uint32_t mip = 0; mip < levels; mip++) {
            upload_texture(made.resource, mip + layer * levels, level, lw * 4u, lh);
            if (mip + 1 == levels) break;
            downsample(level, lw, lh, next);
            uint32_t *swap = scratch;
            scratch = next;
            next = swap;
            level = scratch;
            lw = lw > 1 ? lw / 2 : 1;
            lh = lh > 1 ? lh / 2 : 1;
        }
    }
    release_memory(scratch);
    release_memory(next);
    barrier(made.resource, D3D12_RESOURCE_STATE_COPY_DEST, shader_resource);
    made.srv = views(1);
    D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
    view.Format = format;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (cube) {
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        view.TextureCube.MipLevels = levels;
    } else if (layers > 1) {
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        view.Texture2DArray.MipLevels = levels;
        view.Texture2DArray.ArraySize = layers;
    } else {
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = levels;
    }
    texture_view(made.resource, made.srv.cpu, &view);
    made.view = view;
    return made;
}

Descriptors Gpu::table(const Texture *const *textures, uint32_t count)
{
    const Descriptors run = views(count);
    for (uint32_t i = 0; i < count && run.count != 0; i++)
        texture_view(textures[i]->resource, run.cpu_at(i), &textures[i]->view);
    return run;
}

Upload Gpu::upload(uint32_t bytes, uint32_t alignment)
{
    Slot &s = slots_[slot_];
    Upload chunk;
    const uint64_t at = align_up(s.ring_used, alignment);
    if (at + bytes > ring_bytes) {
        log("FAIL: the frame's upload ring is full\n");
        failed = true;
        return chunk;
    }
    s.ring_used = at + bytes;
    chunk.cpu = s.ring_cpu + at;
    chunk.gpu = s.ring_gpu + at;
    chunk.resource = s.ring;
    chunk.offset = at;
    return chunk;
}

D3D12_GPU_VIRTUAL_ADDRESS Gpu::constants(const void *data, uint32_t bytes)
{
    const Upload chunk = upload(static_cast<uint32_t>(align_up(bytes, 256)));
    if (chunk.cpu == nullptr) return 0;
    copy_bytes(chunk.cpu, data, bytes);
    return chunk.gpu;
}

// ---- descriptors

Descriptors Gpu::views(uint32_t count)
{
    const Descriptors run = views_.take(count);
    if (run.count == 0) failed = true;
    return run;
}

Descriptors Gpu::frame_views(uint32_t count)
{
    Descriptors run;
    if (frame_views_used_ + count > views_per_slot) {
        failed = true;
        return run;
    }
    const uint32_t first = static_views + slot_ * views_per_slot + frame_views_used_;
    frame_views_used_ += count;
    run.stride = views_.stride;
    run.count = count;
    run.cpu = views_.heap->GetCPUDescriptorHandleForHeapStart();
    run.cpu.ptr += static_cast<SIZE_T>(first) * views_.stride;
    run.gpu = views_.heap->GetGPUDescriptorHandleForHeapStart();
    run.gpu.ptr += static_cast<UINT64>(first) * views_.stride;
    return run;
}

Descriptors Gpu::sampler_views(uint32_t count)
{
    const Descriptors run = samplers_.take(count);
    if (run.count == 0) failed = true;
    return run;
}

Descriptors Gpu::cpu_views(uint32_t count)
{
    const Descriptors run = cpu_views_.take(count);
    if (run.count == 0) failed = true;
    return run;
}

Descriptors Gpu::rtvs(uint32_t count)
{
    const Descriptors run = rtvs_.take(count);
    if (run.count == 0) failed = true;
    return run;
}

Descriptors Gpu::dsvs(uint32_t count)
{
    const Descriptors run = dsvs_.take(count);
    if (run.count == 0) failed = true;
    return run;
}

void Gpu::texture_view(ID3D12Resource *viewed, D3D12_CPU_DESCRIPTOR_HANDLE at, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc)
{
    if (viewed == nullptr || at.ptr == 0) {
        failed = true;
        return;
    }
    device->CreateShaderResourceView(viewed, desc, at);
}

void Gpu::make_sampler(D3D12_CPU_DESCRIPTOR_HANDLE at, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address)
{
    D3D12_SAMPLER_DESC desc = {};
    desc.Filter = filter;
    desc.AddressU = desc.AddressV = desc.AddressW = address;
    desc.MaxAnisotropy = 8;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    device->CreateSampler(&desc, at);
}

// ---- recording

void Gpu::barrier(ID3D12Resource *resource_, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                  uint32_t subresource)
{
    if (resource_ == nullptr || before == after) return;
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = resource_;
    b.Transition.Subresource = subresource;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    list->ResourceBarrier(1, &b);
}

void Gpu::uav_barrier(ID3D12Resource *resource_)
{
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = resource_;
    list->ResourceBarrier(1, &b);
}

void Gpu::bind_frame()
{
    list->OMSetRenderTargets(1, &target, FALSE, &depth);
    viewport(0, 0, static_cast<float>(width), static_cast<float>(height));
}

void Gpu::begin_frame(float r, float g, float b)
{
    const float clear[4] = {r, g, b, 1.0f};
    list->OMSetRenderTargets(1, &target, FALSE, &depth);
    list->ClearRenderTargetView(target, clear, 0, nullptr);
    list->ClearDepthStencilView(depth, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    viewport(0, 0, static_cast<float>(width), static_cast<float>(height));
}

void Gpu::viewport(float x, float y, float w, float h)
{
    const D3D12_VIEWPORT view = {x, y, w, h, 0.0f, 1.0f};
    list->RSSetViewports(1, &view);
    const D3D12_RECT scissor = {static_cast<LONG>(x), static_cast<LONG>(y), static_cast<LONG>(x + w),
                                static_cast<LONG>(y + h)};
    list->RSSetScissorRects(1, &scissor);
}

}  // namespace samples
