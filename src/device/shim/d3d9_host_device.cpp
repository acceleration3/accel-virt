#include "device/shim/d3d9_host_device.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/d3d_runtime.hpp"
#include "device/shim/physical_device_identity.hpp"

#include <vulkan/vulkan.h>

namespace av {

namespace {

/* What a device with no window still needs: one back buffer, which nothing draws. */
constexpr UINT unused_back_buffer = 16u;
/* What every check is made against: the desktop's format. */
constexpr D3DFORMAT display = D3DFMT_X8R8G8B8;
constexpr uint32_t  last_multisample = D3DMULTISAMPLE_16_SAMPLES;

/* QueryFormats9's bits (d3d9.toml), each a CheckDeviceFormat question. */
struct FormatCheck {
    uint32_t usage;
    uint32_t type;
};
constexpr FormatCheck format_checks[] = {
    {0u, D3DRTYPE_TEXTURE},
    {0u, D3DRTYPE_CUBETEXTURE},
    {0u, D3DRTYPE_VOLUMETEXTURE},
    {D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE},
    {D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE},
    {D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE},
    {D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE},
    {D3DUSAGE_QUERY_SRGBREAD, D3DRTYPE_TEXTURE},
    {D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_TEXTURE},
    {D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE},
    {D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE},
    {D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE},
};
constexpr uint32_t back_buffer_bit = 12u;
constexpr uint32_t offscreen_bit = 13u;
constexpr uint32_t bump_bit = 14u;
constexpr uint32_t depth_texture_bit = 15u;

}  // namespace

D3d9HostDevice::D3d9HostDevice() : d3d9_(nullptr), adapter_(~0u), memory_mb_(0u)
{
    AV_SCOPE("D3d9HostDevice::D3d9HostDevice");
}

D3d9HostDevice::~D3d9HostDevice()
{
    AV_SCOPE("D3d9HostDevice::~D3d9HostDevice");
    close();
}

bool D3d9HostDevice::open(const D3dRuntime &runtime, const uint8_t *uuid)
{
    AV_SCOPE("D3d9HostDevice::open");
    if (d3d9_ != nullptr) return true;
    d3d9_ = runtime.create_d3d9();
    if (d3d9_ == nullptr) return false;
    adapter_ = adapter_for(uuid);
    if (adapter_ != ~0u) return true;
    close();
    return false;
}

void D3d9HostDevice::close()
{
    AV_SCOPE("D3d9HostDevice::close");
    if (d3d9_ != nullptr) d3d9_->Release();
    d3d9_ = nullptr;
    adapter_ = ~0u;
    memory_mb_ = 0u;
}

bool D3d9HostDevice::opened() const
{
    AV_SCOPE("D3d9HostDevice::opened");
    return d3d9_ != nullptr;
}

/* The adapter found also says how much device-local memory it has, through
 * the same instance: a physical device is only meaningful to its own. */
uint32_t D3d9HostDevice::adapter_for(const uint8_t *uuid)
{
    AV_SCOPE("D3d9HostDevice::adapter_for");
    ID3D9VkInteropInterface *interop = nullptr;
    if (FAILED(d3d9_->QueryInterface(__uuidof(ID3D9VkInteropInterface), reinterpret_cast<void **>(&interop))))
        return ~0u;
    VkInstance instance = VK_NULL_HANDLE;
    interop->GetInstanceHandle(&instance);
    uint32_t found = ~0u;
    for (UINT ordinal = 0u; found == ~0u && ordinal < d3d9_->GetAdapterCount(); ordinal++) {
        VkPhysicalDevice physical = VK_NULL_HANDLE;
        interop->GetPhysicalDeviceHandle(ordinal, &physical);
        if (!PhysicalDeviceIdentity(instance, physical).is(uuid)) continue;
        found = ordinal;
        auto query = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
        VkPhysicalDeviceMemoryProperties memory = {};
        if (query != nullptr) query(physical, &memory);
        uint64_t local = 0u;
        for (uint32_t heap = 0u; heap < memory.memoryHeapCount; heap++)
            if ((memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u)
                local += memory.memoryHeaps[heap].size;
        memory_mb_ = static_cast<uint32_t>(local >> 20u);
    }
    interop->Release();
    return found;
}

/* FPU_PRESERVE: Direct3D 9 otherwise sets the calling thread's FPU to single
 * precision, and the thread is the executor's, not the guest's. MULTITHREADED:
 * the present engine reads a device's frames from a thread of its own. */
int32_t D3d9HostDevice::make_device(IDirect3DDevice9 *&device) const
{
    AV_SCOPE("D3d9HostDevice::make_device");
    device = nullptr;
    if (d3d9_ == nullptr) return D3DERR_NOTAVAILABLE;
    D3DPRESENT_PARAMETERS parameters = {};
    parameters.BackBufferWidth = unused_back_buffer;
    parameters.BackBufferHeight = unused_back_buffer;
    parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    parameters.BackBufferCount = 1u;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.Windowed = TRUE;
    const DWORD behavior = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
    const HRESULT made = d3d9_->CreateDevice(adapter_, D3DDEVTYPE_HAL, nullptr, behavior, &parameters, &device);
    if (FAILED(made)) device = nullptr;
    return made;
}

int32_t D3d9HostDevice::caps(D3DCAPS9 &answer) const
{
    AV_SCOPE("D3d9HostDevice::caps");
    if (d3d9_ == nullptr) return D3DERR_NOTAVAILABLE;
    return d3d9_->GetDeviceCaps(adapter_, D3DDEVTYPE_HAL, &answer);
}

int32_t D3d9HostDevice::identity(D3DADAPTER_IDENTIFIER9 &identifier) const
{
    AV_SCOPE("D3d9HostDevice::identity");
    if (d3d9_ == nullptr) return D3DERR_NOTAVAILABLE;
    return d3d9_->GetAdapterIdentifier(adapter_, 0u, &identifier);
}

uint32_t D3d9HostDevice::video_memory_mb() const
{
    AV_SCOPE("D3d9HostDevice::video_memory_mb");
    return memory_mb_;
}

bool D3d9HostDevice::checks(uint32_t usage, uint32_t type, uint32_t format) const
{
    AV_SCOPE("D3d9HostDevice::checks");
    return d3d9_->CheckDeviceFormat(adapter_, D3DDEVTYPE_HAL, display, usage, static_cast<D3DRESOURCETYPE>(type),
                                    static_cast<D3DFORMAT>(format)) == D3D_OK;
}

void D3d9HostDevice::format_support(uint32_t format, uint32_t &support, uint32_t &multisample) const
{
    AV_SCOPE("D3d9HostDevice::format_support");
    support = 0u;
    multisample = 0u;
    if (d3d9_ == nullptr) return;
    uint32_t bit = 0u;
    for (const FormatCheck &check : format_checks) {
        if (checks(check.usage, check.type, format)) support |= 1u << bit;
        bit++;
    }
    const auto as = static_cast<D3DFORMAT>(format);
    if (d3d9_->CheckDeviceType(adapter_, D3DDEVTYPE_HAL, display, as, TRUE) == D3D_OK) support |= 1u << back_buffer_bit;
    if (checks(0u, D3DRTYPE_SURFACE, format)) support |= 1u << offscreen_bit;
    if (checks(D3DUSAGE_QUERY_LEGACYBUMPMAP, D3DRTYPE_TEXTURE, format)) support |= 1u << bump_bit;
    if (checks(D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, format)) support |= 1u << depth_texture_bit;
    for (uint32_t type = D3DMULTISAMPLE_2_SAMPLES; type <= last_multisample; type++) {
        uint32_t levels = 0u;
        if (this->multisample(format, type, levels) == D3D_OK) multisample |= 1u << type;
    }
}

int32_t D3d9HostDevice::multisample(uint32_t format, uint32_t type, uint32_t &quality_levels) const
{
    AV_SCOPE("D3d9HostDevice::multisample");
    quality_levels = 0u;
    if (d3d9_ == nullptr) return D3DERR_NOTAVAILABLE;
    DWORD levels = 0u;
    const HRESULT answer = d3d9_->CheckDeviceMultiSampleType(adapter_, D3DDEVTYPE_HAL, static_cast<D3DFORMAT>(format),
                                                             TRUE, static_cast<D3DMULTISAMPLE_TYPE>(type), &levels);
    if (SUCCEEDED(answer)) quality_levels = levels;
    return answer;
}

}  // namespace av
