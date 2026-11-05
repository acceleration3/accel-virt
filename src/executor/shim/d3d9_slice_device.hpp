#ifndef AV_D3D9_SLICE_DEVICE_HPP
#define AV_D3D9_SLICE_DEVICE_HPP

#include "executor/shim/slice_importer.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

struct IDirect3DDevice9;
struct IDirect3DResource9;
struct ID3D9VkInteropDevice1;

namespace av {

class D3d9SliceDevice final : public SliceImporter {
public:
    D3d9SliceDevice();
    ~D3d9SliceDevice();
    D3d9SliceDevice(const D3d9SliceDevice &) = delete;
    D3d9SliceDevice &operator=(const D3d9SliceDevice &) = delete;

    [[nodiscard]] bool connect(IDirect3DDevice9 *device);
    bool               connected() const;
    bool               is(const IDirect3DDevice9 *device) const;
    void               idle();
    void               release();

    [[nodiscard]] bool import_to_draw(OwnedFd &descriptor, uint64_t bytes, VulkanBuffer &buffer) override;
    [[nodiscard]] bool bind_storage(IDirect3DResource9 *buffer, const VulkanBuffer &storage, uint64_t offset,
                                    uint64_t bytes, void *host);

    bool invariants_hold() const;

private:
    IDirect3DDevice9      *device_;
    ID3D9VkInteropDevice1 *interop_;
    VulkanDeviceTable      table_;
    VkQueue                queue_;
};

}

#endif
