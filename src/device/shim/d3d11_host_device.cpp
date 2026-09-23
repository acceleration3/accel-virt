#include "device/shim/d3d11_host_device.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/d3d_runtime.hpp"

namespace av {

D3d11HostDevice::D3d11HostDevice()
    : device_(nullptr), context_(nullptr), active_(nullptr), feature_level_(0u), capabilities_(), shared_(), staging_(nullptr), mutex_()
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
    return true;
}

void D3d11HostDevice::close()
{
    AV_SCOPE("D3d11HostDevice::close");
    shared_.clear();
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
bool D3d11HostDevice::read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height)
{
    AV_SCOPE("D3d11HostDevice::read_image");
    if (device_ == nullptr || pixels == nullptr) return false;
    lock(nullptr);
    ID3D11Texture2D *texture = nullptr;
    IUnknown *const published = shared_.find(key);
    const bool found = published != nullptr &&
                       SUCCEEDED(published->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)));
    bool read = false;
    if (found) {
        read = copy_out(texture, static_cast<uint8_t *>(pixels), stride, width, height);
        texture->Release();
    }
    unlock();
    return read;
}

bool D3d11HostDevice::copy_out(ID3D11Texture2D *texture, uint8_t *pixels, uint32_t stride, uint32_t width,
                               uint32_t height)
{
    AV_SCOPE("D3d11HostDevice::copy_out");
    D3D11_TEXTURE2D_DESC wanted = {};
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
        if (FAILED(device_->CreateTexture2D(&made, nullptr, &staging_))) return false;
    }
    context_->CopySubresourceRegion(staging_, 0u, 0u, 0u, 0u, texture, 0u, nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context_->Map(staging_, 0u, D3D11_MAP_READ, 0u, &mapped))) return false;
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
    pthread_mutex_lock(&mutex_);
    if (state == nullptr || state == active_) return;
    context_->SwapDeviceContextState(state, nullptr);
    active_ = state;
}

void D3d11HostDevice::unlock()
{
    AV_SCOPE("D3d11HostDevice::unlock");
    pthread_mutex_unlock(&mutex_);
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

D3dSharedResources &D3d11HostDevice::shared()
{
    AV_SCOPE("D3d11HostDevice::shared");
    return shared_;
}

}  // namespace av
