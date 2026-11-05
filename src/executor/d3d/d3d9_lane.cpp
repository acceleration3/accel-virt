#include "executor/d3d/d3d9_lane.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d9_lanes.hpp"
#include "executor/shim/vulkan_buffer.hpp"

#include <unistd.h>

namespace av {

namespace {

void submission_done(void *lane, uint64_t value)
{
    AV_SCOPE("submission_done");
    static_cast<D3d9Lane *>(lane)->completed(value);
}

}

D3d9Lane::D3d9Lane(IDirect3DDevice9 *device, const D3d9Lanes &lanes)
    : device_(device), lanes_(lanes), interop_(), table_(), issued_(0u), done_(0u), wake_(-1), tried_(false),
      allowed_(false), touched_(false)
{
    AV_SCOPE("D3d9Lane::D3d9Lane");
}

D3d9Lane::~D3d9Lane()
{
    AV_SCOPE("D3d9Lane::~D3d9Lane");
    release();
}

bool D3d9Lane::connect()
{
    AV_SCOPE("D3d9Lane::connect");
    if (tried_) return connected();
    tried_ = true;
    if (!interop_.connect(device_)) return false;
    VkInstance       instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice         vulkan = VK_NULL_HANDLE;
    interop_.handles(instance, physical, vulkan);
    if (table_.load(instance, physical, vulkan) && table_.imports_dma_bufs()) return true;
    release();
    return false;
}

bool D3d9Lane::connected() const
{
    AV_SCOPE("D3d9Lane::connected");
    return interop_.connected() && table_.loaded();
}

bool D3d9Lane::is(const IDirect3DDevice9 *device) const
{
    AV_SCOPE("D3d9Lane::is");
    return device_ == device;
}

bool D3d9Lane::is(const SliceImporter *importer) const
{
    AV_SCOPE("D3d9Lane::is");
    return static_cast<const SliceImporter *>(this) == importer;
}

void D3d9Lane::allow()
{
    AV_SCOPE("D3d9Lane::allow");
    allowed_ = true;
}

bool D3d9Lane::allowed() const
{
    AV_SCOPE("D3d9Lane::allowed");
    return allowed_;
}

void D3d9Lane::touch()
{
    AV_SCOPE("D3d9Lane::touch");
    touched_ = true;
}

bool D3d9Lane::touched() const
{
    AV_SCOPE("D3d9Lane::touched");
    return touched_;
}

bool D3d9Lane::mark(uint64_t value, int wake)
{
    AV_SCOPE("D3d9Lane::mark");
    touched_ = false;
    __atomic_store_n(&wake_, wake, __ATOMIC_RELAXED);
    if (!connected() || !interop_.mark_submission(&submission_done, this, value)) return false;
    issued_ = value;
    return true;
}

void D3d9Lane::completed(uint64_t value)
{
    AV_SCOPE("D3d9Lane::completed");
    uint64_t seen = __atomic_load_n(&done_, __ATOMIC_RELAXED);
    while (seen < value && !__atomic_compare_exchange_n(&done_, &seen, value, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
    }
    const uint64_t one = 1u;
    const int      wake = __atomic_load_n(&wake_, __ATOMIC_RELAXED);
    if (wake > 0) static_cast<void>(write(wake, &one, sizeof one) < 0);
}

bool D3d9Lane::polled() const
{
    AV_SCOPE("D3d9Lane::polled");
    return __atomic_load_n(&wake_, __ATOMIC_RELAXED) <= 0 && __atomic_load_n(&done_, __ATOMIC_ACQUIRE) < issued_;
}

void D3d9Lane::idle() const
{
    AV_SCOPE("D3d9Lane::idle");
    interop_.wait_for_idle();
}

void D3d9Lane::release()
{
    AV_SCOPE("D3d9Lane::release");
    if (!interop_.connected()) return;
    interop_.wait_for_idle();
    table_.forget();
    interop_.release();
}

bool D3d9Lane::import_to_draw(OwnedFd &descriptor, uint64_t bytes, VulkanBuffer &buffer)
{
    AV_SCOPE("D3d9Lane::import_to_draw");
    return connected() && buffer.import_dma_buf_to_draw(table_, descriptor, bytes);
}

bool D3d9Lane::bindable(IDirect3DResource9 *buffer) const
{
    AV_SCOPE("D3d9Lane::bindable");
    return connected() && interop_.bindable(buffer);
}

bool D3d9Lane::bind_storage(IDirect3DResource9 *buffer, const VulkanBuffer &storage, uint64_t offset,
                            uint64_t bytes, void *host) const
{
    AV_SCOPE("D3d9Lane::bind_storage");
    return connected() && buffer != nullptr && storage.made() && interop_.bind_storage(buffer, storage, offset, bytes, host);
}

bool D3d9Lane::unbind_storage(IDirect3DResource9 *buffer) const
{
    AV_SCOPE("D3d9Lane::unbind_storage");
    return connected() && buffer != nullptr && interop_.unbind_storage(buffer);
}

uint64_t D3d9Lane::next() const
{
    AV_SCOPE("D3d9Lane::next");
    return lanes_.next();
}

uint64_t D3d9Lane::reached() const
{
    AV_SCOPE("D3d9Lane::reached");
    const uint64_t done = __atomic_load_n(&done_, __ATOMIC_ACQUIRE);
    return done >= issued_ ? lanes_.marked() : done;
}

bool D3d9Lane::invariants_hold() const
{
    AV_SCOPE("D3d9Lane::invariants_hold");
    return device_ != nullptr && issued_ <= lanes_.marked() && (!interop_.connected() || table_.loaded());
}

}
