#ifndef AV_D3D9_INTEROP_HPP
#define AV_D3D9_INTEROP_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

struct IDirect3DDevice9;
struct IDirect3DResource9;
struct ID3D9VkInteropDevice1;

namespace av {

class VulkanBuffer;

class D3d9Interop {
public:
    D3d9Interop();
    ~D3d9Interop();
    D3d9Interop(const D3d9Interop &) = delete;
    D3d9Interop &operator=(const D3d9Interop &) = delete;

    [[nodiscard]] bool connect(IDirect3DDevice9 *device);
    void               release();
    bool               connected() const;
    void               handles(VkInstance &instance, VkPhysicalDevice &physical, VkDevice &device) const;

    [[nodiscard]] bool bindable(IDirect3DResource9 *buffer) const;
    [[nodiscard]] bool bind_storage(IDirect3DResource9 *buffer, const VulkanBuffer &storage, uint64_t offset,
                                    uint64_t bytes, void *host) const;
    [[nodiscard]] bool unbind_storage(IDirect3DResource9 *buffer) const;
    [[nodiscard]] bool mark_submission(void (*done)(void *context, uint64_t value), void *context,
                                       uint64_t value) const;
    void               wait_for_idle() const;

private:
    ID3D9VkInteropDevice1 *interop_;
};

}

#endif
