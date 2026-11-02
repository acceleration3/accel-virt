#pragma once

#ifndef _WIN32
#define WIDL_EXPLICIT_AGGREGATE_RETURNS
#endif

#include <windows.h>
#include <d3d12.h>
#include <stddef.h>
#include <stdint.h>

#include "../d3d11/smath.h"

namespace samples {
using sm::pi; using sm::sqrt; using sm::abs; using sm::floor; using sm::fract; using sm::sin; using sm::cos;
using sm::tan; using sm::radians; using sm::min; using sm::max; using sm::clamp; using sm::mix; using sm::Random;
using sm::float2; using sm::float3; using sm::float4; using sm::float4x4; using sm::dot; using sm::cross;
using sm::length; using sm::normalize; using sm::translate; using sm::scale; using sm::rotate_x; using sm::rotate_y;
using sm::rotate_z; using sm::perspective; using sm::orthographic; using sm::look_at;

struct Blob {
    void  *data = nullptr;
    size_t size = 0;
};
void   log(const char *text);
void   log_number(const char *label, double value);
void  *allocate(size_t bytes);
void   release_memory(void *memory);
double seconds();
bool   write_file(const char *path, const void *data, size_t size);
bool   read_file(const char *path, Blob &blob);
bool   shader(const char *name, const char *source, const char *entry, const char *target, Blob &bytecode);
void   set_shader_directory(const char *directory);
bool   serialize_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc, Blob &blob);
struct Thread;
Thread *start_thread(void (*entry)(void *), void *argument);
void    join_thread(Thread *thread);
struct Signal;
Signal *make_signal();
void    raise_signal(Signal *signal);
void    wait_signal(Signal *signal);
void    release_signal(Signal *signal);

void  copy_text(char *to, size_t capacity, const char *from);
void  append_text(char *to, size_t capacity, const char *from);
void  append_number(char *to, size_t capacity, double value, int decimals);
bool  same_text(const char *a, const char *b);
void  copy_bytes(void *to, const void *from, size_t bytes);

template <typename T> void safe_release(T *&object)
{
    if (object != nullptr) object->Release();
    object = nullptr;
}

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    float3 color;
};
struct Mesh {
    Vertex   *vertices = nullptr;
    uint32_t *indices = nullptr;
    uint32_t  vertex_count = 0;
    uint32_t  index_count = 0;
    void      release();
};
Mesh cube_mesh(float size);
Mesh sphere_mesh(float radius, uint32_t slices, uint32_t stacks);
Mesh torus_mesh(float radius, float tube, uint32_t rings, uint32_t sides);
Mesh plane_mesh(float size, uint32_t cells, float uv_scale);
Mesh gear_mesh(float inner, float outer, float width, uint32_t teeth, float tooth_depth, float3 color);

constexpr D3D12_RESOURCE_STATES shader_resource = static_cast<D3D12_RESOURCE_STATES>(
    static_cast<int>(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) |
    static_cast<int>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

struct Descriptors {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
    uint32_t                    count = 0;
    uint32_t                    stride = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_at(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_at(uint32_t index) const;
};

struct DescriptorHeap {
    ID3D12DescriptorHeap *heap = nullptr;
    uint32_t              stride = 0;
    uint32_t              capacity = 0;
    uint32_t              used = 0;
    bool                  shader_visible = false;
    bool                  make(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool visible);
    Descriptors           take(uint32_t count);
    void                  release();
};

struct DrawMesh {
    ID3D12Resource          *vertices = nullptr;
    ID3D12Resource          *indices = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertex_view = {};
    D3D12_INDEX_BUFFER_VIEW  index_view = {};
    uint32_t                 index_count = 0;
    void                     release();
    void                     draw(ID3D12GraphicsCommandList *list, uint32_t instances = 1) const;
};

struct Texture {
    ID3D12Resource                 *resource = nullptr;
    Descriptors                     srv;
    D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
    void                            release();
};

struct Upload {
    void                     *cpu = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
    ID3D12Resource           *resource = nullptr;
    uint64_t                  offset = 0;
};

class RootLayout {
public:
    RootLayout &cbv(uint32_t reg, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootLayout &constants(uint32_t reg, uint32_t values, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootLayout &srv(uint32_t reg, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootLayout &uav(uint32_t reg, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootLayout &srv_table(uint32_t reg, uint32_t count, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootLayout &uav_table(uint32_t reg, uint32_t count, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootLayout &sampler_table(uint32_t reg, uint32_t count,
                              D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL);
    RootLayout &sampler(uint32_t reg, D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC,
                        D3D12_TEXTURE_ADDRESS_MODE address = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                        D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL,
                        D3D12_COMPARISON_FUNC comparison = D3D12_COMPARISON_FUNC_NEVER);
    RootLayout &no_input();

    D3D12_ROOT_PARAMETER      params[16] = {};
    D3D12_DESCRIPTOR_RANGE    ranges[16] = {};
    D3D12_STATIC_SAMPLER_DESC samplers[8] = {};
    uint32_t                  param_count = 0;
    uint32_t                  range_count = 0;
    uint32_t                  sampler_count = 0;
    bool                      input = true;

private:
    RootLayout &table(D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t reg, uint32_t count,
                      D3D12_SHADER_VISIBILITY visibility);
    RootLayout &descriptor(D3D12_ROOT_PARAMETER_TYPE type, uint32_t reg, D3D12_SHADER_VISIBILITY visibility);
};

class Gpu {
public:
    static constexpr uint32_t slots = 3;

    ID3D12Device              *device = nullptr;
    ID3D12CommandQueue        *queue = nullptr;
    ID3D12GraphicsCommandList *list = nullptr;
    uint32_t                   width = 0;
    uint32_t                   height = 0;
    double                     waited = 0.0;
    uint32_t                   waits = 0;
    double                     submitting = 0.0;
    double                     recording = 0.0;
    double                     signalling = 0.0;
    double                     opened_at = 0.0;
    uint64_t                   stamp_frequency = 0;
    double                     gpu_time = 0.0;
    uint32_t                   gpu_frames = 0;
    bool                       spin_waits = false;
    bool                       shared_fence = false;
    ID3D12Resource            *color = nullptr;
    ID3D12Resource            *depth_buffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE target = {};
    D3D12_CPU_DESCRIPTOR_HANDLE depth = {};
    static constexpr DXGI_FORMAT color_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    static constexpr DXGI_FORMAT depth_format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    static const D3D12_INPUT_ELEMENT_DESC mesh_layout[4];

    bool initialize();
    void shutdown();
    void open();
    void submit();
    void finish();
    void reset_sample_state();

    uint32_t slot() const { return slot_; }
    uint64_t submitted() const { return fence_value_; }
    bool     reached(uint64_t value) const;
    void     wait(uint64_t value);
    void     queue_lists(ID3D12CommandList *const *lists, uint32_t count);

    D3D12_SHADER_BYTECODE vs(const char *name, const char *source, const char *entry = "vs");
    D3D12_SHADER_BYTECODE ps(const char *name, const char *source, const char *entry = "ps");
    D3D12_SHADER_BYTECODE gs(const char *name, const char *source, const char *entry = "gs");
    D3D12_SHADER_BYTECODE hs(const char *name, const char *source, const char *entry = "hs");
    D3D12_SHADER_BYTECODE ds(const char *name, const char *source, const char *entry = "ds");
    D3D12_SHADER_BYTECODE cs(const char *name, const char *source, const char *entry = "cs");

    ID3D12RootSignature *root_signature(const RootLayout &layout);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics(ID3D12RootSignature *root, D3D12_SHADER_BYTECODE vs,
                                                D3D12_SHADER_BYTECODE ps);
    ID3D12PipelineState *pipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc);
    ID3D12PipelineState *compute(ID3D12RootSignature *root, D3D12_SHADER_BYTECODE cs);

    ID3D12Resource *buffer(const void *data, uint32_t bytes, D3D12_RESOURCE_STATES state,
                           D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    ID3D12Resource *resource(const D3D12_RESOURCE_DESC &desc, D3D12_RESOURCE_STATES state,
                             const D3D12_CLEAR_VALUE *clear = nullptr);
    DrawMesh        mesh(const Mesh &mesh);
    Texture         texture(const uint32_t *texels, uint32_t width, uint32_t height, uint32_t layers = 1,
                            bool cube = false, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, bool mips = true);
    void            upload_texture(ID3D12Resource *target, uint32_t subresource, const void *data, uint32_t row_bytes,
                                   uint32_t rows, uint32_t depth = 1);
    Upload                    upload(uint32_t bytes, uint32_t alignment = 256);
    D3D12_GPU_VIRTUAL_ADDRESS constants(const void *data, uint32_t bytes);
    void                      keep(ID3D12Resource *resource);

    Descriptors views(uint32_t count);
    Descriptors frame_views(uint32_t count);
    Descriptors sampler_views(uint32_t count);
    Descriptors cpu_views(uint32_t count);
    Descriptors rtvs(uint32_t count);
    Descriptors dsvs(uint32_t count);
    Descriptors table(const Texture *const *textures, uint32_t count);
    void        texture_view(ID3D12Resource *resource, D3D12_CPU_DESCRIPTOR_HANDLE at,
                             const D3D12_SHADER_RESOURCE_VIEW_DESC *desc = nullptr);
    void        make_sampler(D3D12_CPU_DESCRIPTOR_HANDLE at, D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC,
                             D3D12_TEXTURE_ADDRESS_MODE address = D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    void barrier(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
                 uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void uav_barrier(ID3D12Resource *resource);

    void  begin_frame(float r, float g, float b);
    void  bind_frame();
    void  viewport(float x, float y, float w, float h);
    float aspect() const { return static_cast<float>(width) / static_cast<float>(height); }
    bool  failed = false;

private:
    struct Slot {
        ID3D12CommandAllocator *allocator = nullptr;
        uint64_t                fence = 0;
        ID3D12Resource         *ring = nullptr;
        uint8_t                *ring_cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS ring_gpu = 0;
        uint64_t                ring_used = 0;
        bool                    stamped = false;
    };
    struct CachedShader {
        char                  name[64];
        D3D12_SHADER_BYTECODE code;
    };
    D3D12_SHADER_BYTECODE shader_code(const char *name, const char *source, const char *entry, const char *target);

    Slot                 slots_[slots];

    ID3D12QueryHeap *stamps_ = nullptr;

    ID3D12Resource  *stamp_readback_ = nullptr;

    const uint64_t  *stamp_cpu_ = nullptr;

    double           stamp_seconds_ = 0.0;
    uint32_t             slot_ = 0;
    ID3D12Fence         *fence_ = nullptr;
    uint64_t             fence_value_ = 0;
    DescriptorHeap       views_, samplers_, cpu_views_, rtvs_, dsvs_;
    uint32_t             frame_views_used_ = 0;
    ID3D12Resource      *kept_[1024] = {};
    uint32_t             kept_count_ = 0;
    ID3D12CommandList   *queued_[16] = {};
    uint32_t             queued_count_ = 0;
    CachedShader        *shaders_ = nullptr;
    uint32_t             shader_count_ = 0;
    bool                 recording_ = false;
};

class Sample {
public:
    virtual const char *name() const = 0;
    virtual bool        init(Gpu &gpu) = 0;
    virtual void        render(Gpu &gpu, float time) = 0;
    virtual void        release() = 0;
};

Sample **registry(uint32_t &count);

bool create_device(ID3D12Device **device);
int  run(int argc, const char *const *argv);

}
