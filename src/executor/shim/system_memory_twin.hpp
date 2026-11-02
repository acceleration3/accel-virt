#ifndef AV_SYSTEM_MEMORY_TWIN_HPP
#define AV_SYSTEM_MEMORY_TWIN_HPP

#include "device/abi/av_executor.h"
#include "executor/shim/texture_hold.hpp"
#include "executor/shim/vulkan_image.hpp"

#include <stdint.h>

struct ID3D11DeviceContext4;
struct ID3D11Texture2D;

namespace av {

class DxvkVulkanDevice;

class SystemMemoryTwin final : public TextureHold {
public:
    explicit SystemMemoryTwin(ExportableImages &owner);
    ~SystemMemoryTwin();
    SystemMemoryTwin(const SystemMemoryTwin &) = delete;
    SystemMemoryTwin &operator=(const SystemMemoryTwin &) = delete;

    [[nodiscard]] bool make(DxvkVulkanDevice &vulkan, ID3D11Texture2D *source);
    bool               made() const;
    void               copy_from(ID3D11DeviceContext4 *context, ID3D11Texture2D *source, uint64_t writes);
    [[nodiscard]] bool describe(AvExportedImage &described) const;
    void               free();

private:
    VulkanImage      image_;
    ID3D11Texture2D *texture_;
    uint32_t         width_;
    uint32_t         height_;
    uint32_t         format_;
    uint64_t         copied_writes_;
    bool             copied_;
};

}

#endif
