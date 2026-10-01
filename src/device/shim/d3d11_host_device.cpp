#include "device/shim/d3d11_host_device.hpp"

#include <stdio.h>
#include <time.h>

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d12_host_device.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/d3d_runtime.hpp"
#include "device/shim/vkd3d_image_export.hpp"

namespace av {



D3d11HostDevice::D3d11HostDevice()
    : device_(nullptr), device12_(nullptr), context_(nullptr), active_(nullptr), feature_level_(0u), capabilities_(), shared_(), vulkan_(), exports_(vulkan_), interop_(vulkan_), frames_(*this), staging_(nullptr), mutex_(), waiting_(0u), taken_(0u)
{
    AV_SCOPE("D3d11HostDevice::D3d11HostDevice");
    pthread_mutex_init(&mutex_, nullptr);
}

D3d11HostDevice::~D3d11HostDevice()
{
    AV_SCOPE("D3d11HostDevice::~D3d11HostDevice");
    close();
    pthread_mutex_destroy(&mutex_);
}

bool D3d11HostDevice::open(const D3dRuntime &runtime, const uint8_t *uuid)
{
    AV_SCOPE("D3d11HostDevice::open");
    if (device_ != nullptr) return false;
    IDXGIAdapter1 *const adapter = runtime.adapter_for(uuid);
    if (adapter == nullptr) return false;
    DXGI_ADAPTER_DESC1 description = {};
    adapter->GetDesc1(&description);
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    uint32_t level = 0u;
    const HRESULT made = runtime.create_device11(adapter, device, context, level);
    adapter->Release();
    if (FAILED(made)) return false;
    /* DXVK's device is every revision at once; the newest is asked for so the
     * backend can call anything up to Direct3D 11.4 without asking again. */
    const bool upgraded =
        SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Device5), reinterpret_cast<void **>(&device_))) &&
        SUCCEEDED(context->QueryInterface(__uuidof(ID3D11DeviceContext4), reinterpret_cast<void **>(&context_)));
    device->Release();
    context->Release();
    if (!upgraded) {
        close();
        return false;
    }
    feature_level_ = level;
    capabilities_.measure(*device_, level, description.VendorId, description.DeviceId,
                          description.DedicatedVideoMemory);
    /* Without DXVK's Vulkan interop the scanout is read back rather than
     * exported, and a Direct3D 12 swap chain has nothing to be shown by. */
    if (!vulkan_.connect(device_))
        fprintf(stderr, "virtio-gpu-av: d3d11: DXVK offers no Vulkan interop with fd export; "
                        "Direct3D 12 swap chains cannot be shown\n");
    return true;
}

void D3d11HostDevice::close()
{
    AV_SCOPE("D3d11HostDevice::close");
    exports_.release();
    shared_.clear();
    if (context_ != nullptr) frames_.clear();
    interop_.release();
    vulkan_.release();
    if (staging_ != nullptr) staging_->Release();
    staging_ = nullptr;
    if (context_ != nullptr) context_->Release();
    if (device_ != nullptr) device_->Release();
    context_ = nullptr;
    device_ = nullptr;
    active_ = nullptr;
}

/* Copying and mapping use no pipeline state, so whichever session's is in
 * force stays. The copy waits for everything the sessions submitted before it,
 * the frame the scanout names included. */
/* The copy is queued under the lock and waited for outside it: the wait is
 * the GPU finishing everything queued ahead, and every session's commands
 * need the lock meanwhile. */
bool D3d11HostDevice::read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height)
{
    AV_SCOPE("D3d11HostDevice::read_image");
    if (device_ == nullptr || pixels == nullptr) return false;
    lock(nullptr);
    D3D11_TEXTURE2D_DESC wanted = {};
    const bool staged = stage(key, wanted);
    unlock();
    if (!staged) return false;
    return drain(wanted, static_cast<uint8_t *>(pixels), stride, width, height);
}

/* Queued under the lock and waited for outside it, as a readback is. */
bool D3d11HostDevice::export_image(uint64_t key, AvExportedImage &image)
{
    AV_SCOPE("D3d11HostDevice::export_image");
    constexpr long     poll_nanoseconds = 50000;
    constexpr uint32_t most_polls = 400u;   /* 20 ms */
    if (device_ == nullptr) return false;
    lock(nullptr);
    IUnknown        *const published = find_shared(key);
    ID3D11Texture2D *texture = nullptr;
    const bool found = published != nullptr &&
                       SUCCEEDED(published->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)));
    if (published != nullptr) published->Release();
    uint32_t   slot = 0u;
    const bool staged = found && exports_.stage(device_, context_, texture, slot);
    if (texture != nullptr) texture->Release();
    unlock();
    if (!staged) return false;
    for (uint32_t poll = 0u; poll < most_polls; poll++) {
        lock(nullptr);
        const bool done = exports_.copied(context_, slot);
        unlock();
        if (done) break;
        const timespec pause = {0, poll_nanoseconds};
        nanosleep(&pause, nullptr);
    }
    exports_.describe(slot, image);
    return true;
}

StagedFrame *D3d11HostDevice::stage_frame(ID3D11Texture2D *source, uint32_t subresource, uint32_t width,
                                          uint32_t height, uint32_t format)
{
    AV_SCOPE("D3d11HostDevice::stage_frame");
    return device_ != nullptr ? frames_.stage(source, subresource, width, height, format) : nullptr;
}

void D3d11HostDevice::release_shared(uint64_t object_id)
{
    AV_SCOPE("D3d11HostDevice::release_shared");
    lock(nullptr);
    /* A reset is rare and ends every id the guest had: the log says it came,
     * since a surface held past it would be refused its key again. */
    if (object_id == 0u) {
        fprintf(stderr, "virtio-gpu-av: d3d11: the guest's shared ids start again; %u released\n", shared_.count());
        shared_.clear();
    } else {
        shared_.release(object_id);
    }
    unlock();
}

bool D3d11HostDevice::stage(uint64_t key, D3D11_TEXTURE2D_DESC &wanted)
{
    AV_SCOPE("D3d11HostDevice::stage");
    IUnknown *const published = find_shared(key);
    if (published == nullptr) return false;
    ID3D11Texture2D *texture = nullptr;
    const bool found = SUCCEEDED(published->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)));
    published->Release();
    if (!found) return false;
    texture->GetDesc(&wanted);
    D3D11_TEXTURE2D_DESC have = {};
    if (staging_ != nullptr) staging_->GetDesc(&have);
    if (staging_ == nullptr || have.Width != wanted.Width || have.Height != wanted.Height || have.Format != wanted.Format) {
        if (staging_ != nullptr) staging_->Release();
        staging_ = nullptr;
        D3D11_TEXTURE2D_DESC made = wanted;
        made.MipLevels = 1u;
        made.ArraySize = 1u;
        made.SampleDesc = {1u, 0u};
        made.Usage = D3D11_USAGE_STAGING;
        made.BindFlags = 0u;
        made.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        made.MiscFlags = 0u;
        if (FAILED(device_->CreateTexture2D(&made, nullptr, &staging_))) {
            texture->Release();
            return false;
        }
    }
    context_->CopySubresourceRegion(staging_, 0u, 0u, 0u, 0u, texture, 0u, nullptr);
    context_->Flush();
    texture->Release();
    return true;
}

bool D3d11HostDevice::drain(const D3D11_TEXTURE2D_DESC &wanted, uint8_t *pixels, uint32_t stride, uint32_t width,
                            uint32_t height)
{
    AV_SCOPE("D3d11HostDevice::drain");
    constexpr long poll_nanoseconds = 200000;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    for (;;) {
        lock(nullptr);
        const HRESULT result = context_->Map(staging_, 0u, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
        if (SUCCEEDED(result)) break;
        unlock();
        if (result != DXGI_ERROR_WAS_STILL_DRAWING) return false;
        const timespec pause = {0, poll_nanoseconds};
        nanosleep(&pause, nullptr);
    }
    const uint32_t columns = wanted.Width < width ? wanted.Width : width;
    const uint32_t rows = wanted.Height < height ? wanted.Height : height;
    const bool swap = wanted.Format == DXGI_FORMAT_R8G8B8A8_UNORM || wanted.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
                      wanted.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS;
    for (uint32_t row = 0u; row < rows; row++) {
        const uint8_t *from = static_cast<const uint8_t *>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch;
        uint8_t *to = pixels + static_cast<size_t>(row) * stride;
        __builtin_memcpy(to, from, static_cast<size_t>(columns) * 4u);
        if (!swap) continue;
        for (uint32_t column = 0u; column < columns; column++) {
            const uint8_t red = to[column * 4u];
            to[column * 4u] = to[column * 4u + 2u];
            to[column * 4u + 2u] = red;
        }
    }
    context_->Unmap(staging_, 0u);
    unlock();
    return true;
}

bool D3d11HostDevice::opened() const
{
    AV_SCOPE("D3d11HostDevice::opened");
    return device_ != nullptr;
}


void D3d11HostDevice::lock(ID3DDeviceContextState *state)
{
    AV_SCOPE("D3d11HostDevice::lock");
    __atomic_fetch_add(&waiting_, 1u, __ATOMIC_ACQ_REL);
    pthread_mutex_lock(&mutex_);
    __atomic_fetch_sub(&waiting_, 1u, __ATOMIC_ACQ_REL);
    __atomic_fetch_add(&taken_, uint64_t{1}, __ATOMIC_RELEASE);
    select(state);
}

void D3d11HostDevice::select(ID3DDeviceContextState *state)
{
    AV_SCOPE("D3d11HostDevice::select");
    if (state == nullptr || state == active_) return;
    context_->SwapDeviceContextState(state, nullptr);
    active_ = state;
}

void D3d11HostDevice::unlock()
{
    AV_SCOPE("D3d11HostDevice::unlock");
    const uint64_t mine = __atomic_load_n(&taken_, __ATOMIC_ACQUIRE);
    pthread_mutex_unlock(&mutex_);
    if (__atomic_load_n(&waiting_, __ATOMIC_ACQUIRE) == 0u) return;
    /* Bounded: a waiter that is itself descheduled must not stall this one. */
    constexpr uint32_t patience = 100000u;
    for (uint32_t spin = 0u; spin < patience && __atomic_load_n(&taken_, __ATOMIC_ACQUIRE) == mine; spin++)
        __builtin_ia32_pause();
}

ID3DDeviceContextState *D3d11HostDevice::make_state(uint32_t &feature_level)
{
    AV_SCOPE("D3d11HostDevice::make_state");
    feature_level = 0u;
    if (device_ == nullptr) return nullptr;
    const D3D_FEATURE_LEVEL level = static_cast<D3D_FEATURE_LEVEL>(feature_level_);
    D3D_FEATURE_LEVEL chosen = level;
    ID3DDeviceContextState *state = nullptr;
    pthread_mutex_lock(&mutex_);
    const HRESULT made = device_->CreateDeviceContextState(0u, &level, 1u, D3D11_SDK_VERSION,
                                                           __uuidof(ID3D11Device1), &chosen, &state);
    pthread_mutex_unlock(&mutex_);
    if (FAILED(made)) return nullptr;
    feature_level = chosen;
    return state;
}

void D3d11HostDevice::forget_state(ID3DDeviceContextState *state)
{
    AV_SCOPE("D3d11HostDevice::forget_state");
    pthread_mutex_lock(&mutex_);
    if (state != nullptr && state == active_) {
        /* The context holds a reference to the state in force, so the session
         * releasing its own would not free it; a fresh state is swapped in so
         * this one's bindings - resources the session is about to destroy -
         * are let go now rather than at the next session's swap. */
        uint32_t level = 0u;
        const D3D_FEATURE_LEVEL wanted = static_cast<D3D_FEATURE_LEVEL>(feature_level_);
        D3D_FEATURE_LEVEL chosen = wanted;
        ID3DDeviceContextState *blank = nullptr;
        if (SUCCEEDED(device_->CreateDeviceContextState(0u, &wanted, 1u, D3D11_SDK_VERSION,
                                                        __uuidof(ID3D11Device1), &chosen, &blank))) {
            context_->SwapDeviceContextState(blank, nullptr);
            blank->Release();
        }
        static_cast<void>(level);
        active_ = nullptr;
    }
    pthread_mutex_unlock(&mutex_);
}

ID3D11Device5 *D3d11HostDevice::device() const
{
    AV_SCOPE("D3d11HostDevice::device");
    return device_;
}

ID3D11DeviceContext4 *D3d11HostDevice::context() const
{
    AV_SCOPE("D3d11HostDevice::context");
    return context_;
}

const D3d11Capabilities &D3d11HostDevice::capabilities() const
{
    AV_SCOPE("D3d11HostDevice::capabilities");
    return capabilities_;
}

void D3d11HostDevice::pair(D3d12HostDevice *device12)
{
    AV_SCOPE("D3d11HostDevice::pair");
    device12_ = device12;
}

bool D3d11HostDevice::publish_shared(uint64_t key, IUnknown *surface)
{
    AV_SCOPE("D3d11HostDevice::publish_shared");
    if (device_ == nullptr) return false;
    lock(nullptr);
    const bool published = shared_.publish(key, surface);
    unlock();
    if (!published)
        fprintf(stderr, "virtio-gpu-av: d3d11: surface key 0x%016llx not shared: already shared, or %u of %u held\n",
                static_cast<unsigned long long>(key), shared_.count(), D3dSharedResources::capacity);
    return published;
}

bool D3d11HostDevice::shares(uint64_t key)
{
    AV_SCOPE("D3d11HostDevice::shares");
    if (device_ == nullptr) return false;
    lock(nullptr);
    IUnknown *const found = shared_.find(key);
    unlock();
    if (found == nullptr) return false;
    found->Release();
    return true;
}

IUnknown *D3d11HostDevice::find_shared(uint64_t key)
{
    AV_SCOPE("D3d11HostDevice::find_shared");
    IUnknown *const published = shared_.find(key);
    if (published == nullptr) return nullptr;
    ID3D11Resource *own = nullptr;
    if (SUCCEEDED(published->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<void **>(&own)))) {
        own->Release();
        return published;
    }
    ID3D12Resource *resource = nullptr;
    const bool      twelve =
        SUCCEEDED(published->QueryInterface(__uuidof(ID3D12Resource), reinterpret_cast<void **>(&resource)));
    published->Release();
    if (!twelve) return nullptr;
    ID3D11Texture2D *const texture = view_of(resource);
    resource->Release();
    if (texture == nullptr) return nullptr;
    static_cast<void>(shared_.replace(key, texture));
    return texture;
}

/* A swap chain buffer is one 2D image of one mip; what DWM makes of it is a
 * texture it samples, and the view formats vkd3d-proton made it with are what
 * DXVK may view it as. */
ID3D11Texture2D *D3d11HostDevice::view_of(ID3D12Resource *resource)
{
    AV_SCOPE("D3d11HostDevice::view_of");
    const D3D12_RESOURCE_DESC desc = resource->GetDesc();
    if (device12_ == nullptr || desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.MipLevels != 1u ||
        desc.DepthOrArraySize != 1u || desc.SampleDesc.Count != 1u)
        return nullptr;
    Vkd3dImageExport image;
    if (!device12_->export_image(resource, image)) return nullptr;
    D3D11_TEXTURE2D_DESC1 shown = {};
    shown.Width = static_cast<UINT>(desc.Width);
    shown.Height = desc.Height;
    shown.MipLevels = 1u;
    shown.ArraySize = 1u;
    shown.Format = desc.Format;
    shown.SampleDesc = {1u, 0u};
    shown.Usage = D3D11_USAGE_DEFAULT;
    shown.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0) shown.BindFlags |= D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *const texture = interop_.import(image, shown);
    if (texture == nullptr)
        fprintf(stderr,
                "virtio-gpu-av: d3d12: a surface %ux%u format %u flags 0x%x could not be made a Direct3D 11 view\n",
                shown.Width, shown.Height, static_cast<unsigned>(shown.Format), static_cast<unsigned>(desc.Flags));
    return texture;
}

ID3D11Texture2D *D3d11HostDevice::make_shareable(const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("D3d11HostDevice::make_shareable");
    return interop_.make(desc);
}

bool D3d11HostDevice::export_memory(ID3D11Texture2D *texture, int &fd)
{
    AV_SCOPE("D3d11HostDevice::export_memory");
    return interop_.export_memory(texture, fd);
}

DxvkVulkanDevice &D3d11HostDevice::vulkan()
{
    AV_SCOPE("D3d11HostDevice::vulkan");
    return vulkan_;
}

D3dSharedResources &D3d11HostDevice::shared()
{
    AV_SCOPE("D3d11HostDevice::shared");
    return shared_;
}

}  // namespace av
