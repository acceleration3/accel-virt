#include "executor/shim/d3d11_host_device.hpp"

#include "executor/shim/vulkan_buffer.hpp"
#include "executor/shim/dxvk_interop.hpp"

#include <unistd.h>


#include <stdio.h>

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/poll_budget.hpp"
#include "executor/sharing/host_shares.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "executor/shim/vkd3d_image_export.hpp"

namespace av {



D3d11HostDevice::D3d11HostDevice()
    : runtime_(nullptr), uuid_(), tenant_heap_(), tenants_(tenant_heap_), device_(nullptr), device12_(nullptr), compositor_(nullptr), context_(nullptr), active_(nullptr), feature_level_(0u), capabilities_(), shares_(nullptr), vulkan_(), slices_(nullptr), exports_(vulkan_), composer_(), images_(vulkan_), frames_(*this), staging_(nullptr), mutex_(), waiting_(0u), taken_(0u), sink_(stderr), lines_(sink_, "virtio-gpu-av: d3d11: "), clock_(), carry_heap_(), carried_(carry_heap_)
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

bool D3d11HostDevice::open(const D3dRuntime &runtime, const uint8_t *uuid, HostShares &shares)
{
    AV_SCOPE("D3d11HostDevice::open");
    if (runtime_ != nullptr) return false;
    shares_ = &shares;
    runtime_ = &runtime;
    uuid_ = DeviceUuid(uuid);
    if (make()) return true;
    runtime_ = nullptr;
    return false;
}

bool D3d11HostDevice::make()
{
    AV_SCOPE("D3d11HostDevice::make");
    IDXGIAdapter1 *const adapter = runtime_->adapter_for(uuid_.raw());
    if (adapter == nullptr) return false;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    uint32_t level = 0u;
    const HRESULT made = runtime_->create_device11(adapter, device, context, level);
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
        unmake();
        return false;
    }
    feature_level_ = level;
    capabilities_.measure(*device_, level);
    /* Without DXVK's Vulkan interop the scanout is read back rather than
     * exported, and a Direct3D 12 swap chain has nothing to be shown by. */
    if (!vulkan_.connect(device_))
        lines_.write("DXVK offers no Vulkan interop with fd export; Direct3D 12 swap chains cannot be shown");
    if (FAILED(context_->QueryInterface(__uuidof(ID3D11VkExtContext2), reinterpret_cast<void **>(&slices_)))) {
        slices_ = nullptr;
        lines_.write("DXVK cannot rename a buffer to imported storage: mapped buffers are copied to the host");
    }
    return true;
}

void D3d11HostDevice::close()
{
    AV_SCOPE("D3d11HostDevice::close");
    /* Every share of this GPU holds this device's textures: they go first. */
    if (shares_ != nullptr) shares_->clear();
    retire_locked();
    unmake();
    runtime_ = nullptr;
}

void D3d11HostDevice::unmake()
{
    AV_SCOPE("D3d11HostDevice::unmake");
    composer_.release();
    exports_.release();
    if (context_ != nullptr) frames_.clear();
    images_.release();
    if (slices_ != nullptr) slices_->Release();
    slices_ = nullptr;
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
bool D3d11HostDevice::read_image(uint32_t domain, uint64_t id, void *pixels, uint32_t stride, uint32_t width,
                                 uint32_t height)
{
    AV_SCOPE("D3d11HostDevice::read_image");
    if (pixels == nullptr) return false;
    lock(nullptr);
    D3D11_TEXTURE2D_DESC wanted = {};
    const bool staged = usable() && stage(domain, id, wanted);
    unlock();
    if (!staged) return false;
    return drain(wanted, static_cast<uint8_t *>(pixels), stride, width, height);
}

/* Queued under the lock and waited for outside it, as a readback is. */
bool D3d11HostDevice::export_image(uint32_t domain, uint64_t id, AvExportedImage &image)
{
    AV_SCOPE("D3d11HostDevice::export_image");
    lock(nullptr);
    ID3D11Texture2D *const texture = usable() ? presented_texture(domain, id) : nullptr;
    uint32_t               slot = 0u;
    const bool             staged = texture != nullptr && exports_.stage(device_, context_, texture, slot);
    if (texture != nullptr) texture->Release();
    unlock();
    if (!staged || !exported(slot)) return false;
    exports_.describe(slot, image);
    return true;
}

/* An image handed over before its copy is done shows whatever the slot held
 * last - frames from before, which a game the GPU draws slower than the old
 * 20 ms budget showed jumping back and forth. The copy waits behind every
 * frame queued before it, so the wait is as long as a slow frame, and an
 * image not done even then is not shown at all. */
bool D3d11HostDevice::exported(uint32_t slot)
{
    AV_SCOPE("D3d11HostDevice::exported");
    constexpr uint64_t poll_nanoseconds = 50000u;
    constexpr uint64_t deadline_nanoseconds = 250000000u;
    PollBudget         polls(clock_, 0u, poll_nanoseconds, deadline_nanoseconds);
    while (polls.next()) {
        lock(nullptr);
        const bool live = usable();
        const bool done = live && exports_.copied(context_, slot);
        unlock();
        if (done) return true;
        if (!live) return false;
    }
    return false;
}

/* Composed and queued under the lock, waited for outside it, as export_image
 * is. Every plane's image must be there: a frame missing one is not shown. */
bool D3d11HostDevice::compose_planes(uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                     uint32_t height, AvExportedImage &image)
{
    AV_SCOPE("D3d11HostDevice::compose_planes");
    if (count > PlaneComposer::max_planes || (count != 0u && planes == nullptr)) return false;
    lock(nullptr);
    ID3D11Texture2D *textures[PlaneComposer::max_planes] = {};
    bool             found = usable();
    for (uint32_t index = 0u; found && index < count; index++) {
        textures[index] = presented_texture(domain, planes[index].key);
        found = found && textures[index] != nullptr;
    }
    ID3D11Texture2D *const composed =
        found ? composer_.compose(device_, context_, planes, textures, count, width, height) : nullptr;
    for (ID3D11Texture2D *texture : textures)
        if (texture != nullptr) texture->Release();
    uint32_t   slot = 0u;
    const bool staged = composed != nullptr && exports_.stage(device_, context_, composed, slot);
    unlock();
    if (!staged || !exported(slot)) return false;
    exports_.describe(slot, image);
    return true;
}

bool D3d11HostDevice::export_surface(uint32_t domain, uint64_t surface, AvExportedImage &image)
{
    AV_SCOPE("D3d11HostDevice::export_surface");
    image = AvExportedImage{-1, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    lock(nullptr);
    ID3D11Texture2D *const texture = usable() ? shared_texture(domain, surface) : nullptr;
    const bool             exported = texture != nullptr && images_.export_image(texture, image);
    if (texture != nullptr) texture->Release();
    unlock();
    return exported;
}

/* The count is read under the lock every copy into the surface is made and
 * counted under, and the sync_file made before it is let go, so the sync_file
 * covers every copy the count says ran. */
bool D3d11HostDevice::sync_surface(uint32_t domain, uint64_t surface, uint64_t &writes, int &fd)
{
    AV_SCOPE("D3d11HostDevice::sync_surface");
    writes = 0u;
    fd = -1;
    lock(nullptr);
    const bool synced = usable() && shares_->writes(domain, surface, writes) && vulkan_.sync_file(fd);
    unlock();
    return synced;
}

StagedFrame *D3d11HostDevice::stage_frame(ID3D11Texture2D *source, uint32_t subresource, uint32_t width,
                                          uint32_t height, uint32_t format)
{
    AV_SCOPE("D3d11HostDevice::stage_frame");
    return device_ != nullptr ? frames_.stage(source, subresource, width, height, format) : nullptr;
}

void D3d11HostDevice::retire_shares()
{
    AV_SCOPE("D3d11HostDevice::retire_shares");
    lock(nullptr);
    if (device_ != nullptr) retire_locked();
    unlock();
}

void D3d11HostDevice::retire_locked()
{
    AV_SCOPE("D3d11HostDevice::retire_locked");
    constexpr uint32_t batch = 32u;
    IUnknown          *objects[batch];
    if (shares_ == nullptr) return;
    for (uint32_t taken = shares_->take_retired(objects, batch); taken != 0u;
         taken = shares_->take_retired(objects, batch))
        for (uint32_t index = 0u; index < taken; index++) objects[index]->Release();
    images_.collect();
}

bool D3d11HostDevice::stage(uint32_t domain, uint64_t id, D3D11_TEXTURE2D_DESC &wanted)
{
    AV_SCOPE("D3d11HostDevice::stage");
    ID3D11Texture2D *const texture = shared_texture(domain, id);
    if (texture == nullptr) return false;
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
    /* Five seconds still drawing is a hung GPU, as Direct3D 12's drain
     * judges one: the scanout's reader gives up rather than hang. */
    constexpr uint64_t       poll_nanoseconds = 200000u;
    constexpr uint64_t       hung_nanoseconds = 5000000000u;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    PollBudget               polls(clock_, 0u, poll_nanoseconds, hung_nanoseconds);
    bool                     mapped_now = false;
    while (polls.next()) {
        lock(nullptr);
        const HRESULT result = usable() ? context_->Map(staging_, 0u, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)
                                        : DXGI_ERROR_DEVICE_REMOVED;
        mapped_now = SUCCEEDED(result);
        if (mapped_now) break;
        unlock();
        if (result != DXGI_ERROR_WAS_STILL_DRAWING) return false;
    }
    if (!mapped_now) {
        lines_.write("a readback still drawing after five seconds; the GPU is taken for hung");
        return false;
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

bool D3d11HostDevice::placed() const
{
    AV_SCOPE("D3d11HostDevice::placed");
    return runtime_ != nullptr;
}

bool D3d11HostDevice::admit(WakeSignal &wake)
{
    AV_SCOPE("D3d11HostDevice::admit");
    return device_ != nullptr && tenants_.join(wake) == Status::ok;
}

void D3d11HostDevice::dismiss(WakeSignal &wake)
{
    AV_SCOPE("D3d11HostDevice::dismiss");
    tenants_.leave(wake);
}

/* DXVK knows its device is lost once a submission or a wait said so, and
 * answers it as a removed device from then on: asking costs one atomic read. */
bool D3d11HostDevice::lost()
{
    AV_SCOPE("D3d11HostDevice::lost");
    if (tenants_.lost()) return true;
    if (device_ == nullptr || SUCCEEDED(device_->GetDeviceRemovedReason())) return false;
    declare("DXVK reports it removed");
    return true;
}

void D3d11HostDevice::declare(const char *why)
{
    AV_SCOPE("D3d11HostDevice::declare");
    if (!tenants_.lose()) return;
    LogLine(lines_)
        .text("the device was lost (")
        .text(why)
        .text("): its ")
        .number(tenants_.count())
        .text(" sessions are faulted, and it is made again once they are gone")
        .write();
}

bool D3d11HostDevice::usable()
{
    AV_SCOPE("D3d11HostDevice::usable");
    return device_ != nullptr && !lost();
}

bool D3d11HostDevice::renew(uint32_t wait_ms)
{
    AV_SCOPE("D3d11HostDevice::renew");
    if (runtime_ == nullptr) return false;
    /* Looked at under the lock: another session's renew may be remaking it. */
    lock(nullptr);
    const bool fine = usable();
    unlock();
    if (fine) return true;
    if (!tenants_.vacate(wait_ms)) {
        LogLine(lines_).text("sessions are still on the lost device after ").number(wait_ms).text(" ms: not made again yet").write();
        return false;
    }
    lock(nullptr);
    bool made = device_ != nullptr && !tenants_.lost();
    if (!made) {
        /* What anything still holds of the old device goes before it does:
         * the shares' textures under this lock - what each is kept first, to
         * be made again - then everything made on it. */
        carry_shares();
        if (shares_ != nullptr) shares_->lose_direct3d11();
        if (device_ != nullptr) retire_locked();
        unmake();
        made = make();
        const uint32_t restored = restore_shares();
        if (made) static_cast<void>(tenants_.renew());
        LogLine(lines_)
            .text(made ? "the lost device was made again, with " : "the lost device could not be made again; ")
            .number(restored)
            .text(" shared resources")
            .write();
    }
    unlock();
    return made;
}

void D3d11HostDevice::carry_shares()
{
    AV_SCOPE("D3d11HostDevice::carry_shares");
    if (shares_ == nullptr || device_ == nullptr) return;
    shares_->each_direct3d11([this](uint32_t domain, uint64_t id, IUnknown *resource) {
        CarriedShare *carried = nullptr;
        if (carried_.create(carried, domain, id) != Status::ok) return;
        if (!carried->record(resource, images_)) static_cast<void>(carried_.destroy(carried));
    });
}

uint32_t D3d11HostDevice::restore_shares()
{
    AV_SCOPE("D3d11HostDevice::restore_shares");
    uint32_t restored = 0u;
    carried_.destroy_if([this, &restored](const CarriedShare &visited) {
        CarriedShare &carried = const_cast<CarriedShare &>(visited);
        IUnknown *const made = carried.remake(device_, images_);
        if (made == nullptr) return true;
        if (shares_->restore_resource(carried.domain(), carried.id(), made)) restored++;
        made->Release();
        return true;
    });
    return restored;
}

bool D3d11HostDevice::lose()
{
    AV_SCOPE("D3d11HostDevice::lose");
    lock(nullptr);
    const bool tripped = device_ != nullptr && vulkan_.table().lose();
    unlock();
    if (tripped) declare("a test lost it on purpose");
    return tripped;
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
        const D3D_FEATURE_LEVEL wanted = static_cast<D3D_FEATURE_LEVEL>(feature_level_);
        D3D_FEATURE_LEVEL chosen = wanted;
        ID3DDeviceContextState *blank = nullptr;
        if (SUCCEEDED(device_->CreateDeviceContextState(0u, &wanted, 1u, D3D11_SDK_VERSION,
                                                        __uuidof(ID3D11Device1), &chosen, &blank))) {
            context_->SwapDeviceContextState(blank, nullptr);
            blank->Release();
        }
        active_ = nullptr;
    }
    pthread_mutex_unlock(&mutex_);
}

ID3D11Device5 *D3d11HostDevice::device() const
{
    AV_SCOPE("D3d11HostDevice::device");
    return device_;
}

bool D3d11HostDevice::show(uint32_t domain, uint64_t id, ID3D11Texture2D *presented)
{
    AV_SCOPE("D3d11HostDevice::show");
    if (device_ == nullptr || context_ == nullptr || presented == nullptr) return false;
    ID3D11Texture2D *copy = nullptr;
    IUnknown *const  held = shares_->shown(domain, id);
    if (held != nullptr) {
        static_cast<void>(held->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&copy)));
        held->Release();
    }
    if (copy == nullptr) {
        D3D11_TEXTURE2D_DESC desc = {};
        presented->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = (desc.BindFlags & D3D11_BIND_RENDER_TARGET) | D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0u;
        desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &copy)) || copy == nullptr) return false;
        const bool kept = shares_->keep_shown(domain, id, copy);
        retire_locked();
        if (!kept) {
            copy->Release();
            return false;
        }
    }
    context_->CopyResource(copy, presented);
    copy->Release();
    return true;
}

IUnknown *D3d11HostDevice::open_shared(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3d11HostDevice::open_shared");
    if (shares_ == nullptr) return nullptr;
    return viewed(domain, id, shares_->open_resource(domain, id));
}

/* What a present showed of `id` - the copy its flip took - where one was, and
 * the texture itself otherwise: a scanout reads what was presented, never
 * what the application has drawn into the buffer since. */
ID3D11Texture2D *D3d11HostDevice::presented_texture(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3d11HostDevice::presented_texture");
    IUnknown *const shown = shares_ != nullptr ? shares_->shown(domain, id) : nullptr;
    if (shown == nullptr) return shared_texture(domain, id);
    ID3D11Texture2D *texture = nullptr;
    const bool cast = SUCCEEDED(shown->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)));
    shown->Release();
    return cast ? texture : shared_texture(domain, id);
}

ID3D11Texture2D *D3d11HostDevice::shared_texture(uint32_t domain, uint64_t id)
{
    AV_SCOPE("D3d11HostDevice::shared_texture");
    if (shares_ == nullptr) return nullptr;
    IUnknown *const found = viewed(domain, id, shares_->view(domain, id));
    if (found == nullptr) return nullptr;
    ID3D11Texture2D *texture = nullptr;
    const bool cast = SUCCEEDED(found->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)));
    found->Release();
    return cast ? texture : nullptr;
}

IUnknown *D3d11HostDevice::viewed(uint32_t domain, uint64_t id, IUnknown *found)
{
    AV_SCOPE("D3d11HostDevice::viewed");
    if (found == nullptr) return nullptr;
    ID3D12Resource *resource = nullptr;
    if (FAILED(found->QueryInterface(__uuidof(ID3D12Resource), reinterpret_cast<void **>(&resource)))) return found;
    found->Release();
    ID3D11Texture2D *const texture = view_of(resource);
    resource->Release();
    if (texture == nullptr) return nullptr;
    static_cast<void>(shares_->keep_view(domain, id, texture));
    retire_locked();
    return texture;
}

ID3D11Fence *D3d11HostDevice::make_fence(uint64_t initial)
{
    AV_SCOPE("D3d11HostDevice::make_fence");
    ID3D11Fence *fence = nullptr;
    if (device_ == nullptr ||
        FAILED(device_->CreateFence(initial, D3D11_FENCE_FLAG_NONE, __uuidof(ID3D11Fence),
                                    reinterpret_cast<void **>(&fence))))
        return nullptr;
    return fence;
}

bool D3d11HostDevice::signal(ID3D11Fence *fence, uint64_t value)
{
    AV_SCOPE("D3d11HostDevice::signal");
    if (fence == nullptr || context_ == nullptr || FAILED(context_->Signal(fence, value))) return false;
    context_->Flush();
    return true;
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

void D3d11HostDevice::use_compositor(CompositorDevice *compositor)
{
    AV_SCOPE("D3d11HostDevice::use_compositor");
    compositor_ = compositor;
}

CompositorDevice *D3d11HostDevice::compositor() const
{
    AV_SCOPE("D3d11HostDevice::compositor");
    return compositor_;
}

bool D3d11HostDevice::exportable_pitch(ID3D11Texture2D *texture, uint32_t &pitch)
{
    AV_SCOPE("D3d11HostDevice::exportable_pitch");
    return images_.row_pitch(texture, pitch);
}

ID3D11Texture2D *D3d11HostDevice::adopt_exportable(int fd, const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("D3d11HostDevice::adopt_exportable");
    return images_.adopt(fd, desc);
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
    ID3D11Texture2D *const texture = images_.import(image, shown);
    if (texture == nullptr)
        LogLine(lines_)
            .text("a Direct3D 12 surface ")
            .number(shown.Width)
            .text("x")
            .number(shown.Height)
            .text(" format ")
            .number(static_cast<uint32_t>(shown.Format))
            .text(" flags ")
            .hex(static_cast<uint32_t>(desc.Flags), 8u)
            .text(" could not be made a Direct3D 11 view")
            .write();
    return texture;
}

ID3D11Texture2D *D3d11HostDevice::make_rows(const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("D3d11HostDevice::make_rows");
    return images_.make_rows(desc);
}

ID3D11Texture2D *D3d11HostDevice::make_tiled(const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("D3d11HostDevice::make_tiled");
    return images_.make_tiled(desc);
}

bool D3d11HostDevice::export_memory(ID3D11Texture2D *texture, int &fd)
{
    AV_SCOPE("D3d11HostDevice::export_memory");
    return images_.export_memory(texture, fd);
}

DxvkVulkanDevice &D3d11HostDevice::vulkan()
{
    AV_SCOPE("D3d11HostDevice::vulkan");
    return vulkan_;
}

void D3d11HostDevice::report(StateWriter &writer) const
{
    AV_SCOPE("D3d11HostDevice::report");
    images_.report(writer);
}

bool D3d11HostDevice::slices() const
{
    AV_SCOPE("D3d11HostDevice::slices");
    return slices_ != nullptr && vulkan_.connected();
}

bool D3d11HostDevice::import_to_draw(int fd, uint64_t bytes, VulkanBuffer &buffer)
{
    AV_SCOPE("D3d11HostDevice::import_to_draw");
    if (slices()) return buffer.import_dma_buf_to_draw(vulkan_.table(), fd, bytes);
    if (fd >= 0) ::close(fd);
    return false;
}

bool D3d11HostDevice::bind_storage(ID3D11Buffer *buffer, const VulkanBuffer &storage, uint64_t offset, uint64_t bytes,
                                   void *host)
{
    AV_SCOPE("D3d11HostDevice::bind_storage");
    if (slices_ == nullptr || buffer == nullptr || !storage.made()) return false;
    return SUCCEEDED(slices_->BindBufferStorage(buffer, storage.buffer(), offset, bytes, host));
}

}  // namespace av
