#ifndef AV_VULKAN_ADDRESSES_HPP
#define AV_VULKAN_ADDRESSES_HPP

#include "core/support/keyed_slots.hpp"
#include "executor/gpu_address_registry.hpp"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class BlockSource;

class VulkanAddresses {
public:
    struct Limits {
        uint64_t primitives_;
        uint64_t geometries_;
        uint64_t instances_;
        uint64_t dispatch_;
    };

    explicit VulkanAddresses(BlockSource &blocks);
    VulkanAddresses(const VulkanAddresses &) = delete;
    VulkanAddresses &operator=(const VulkanAddresses &) = delete;

    void limits(const Limits &limits);
    const Limits &limits() const;

    [[nodiscard]] bool note_buffer(uint64_t device, uint64_t buffer, uint64_t bytes);
    [[nodiscard]] bool note_address(uint64_t device, uint64_t buffer, uint64_t address);
    void               forget_buffer(uint64_t buffer);
    bool               buffer_bytes(uint64_t device, uint64_t buffer, uint64_t &bytes) const;
    [[nodiscard]] bool note_structure(uint64_t device, uint64_t structure, uint64_t bytes);
    void               forget_structure(uint64_t structure);
    bool               structure_bytes(uint64_t device, uint64_t structure, uint64_t &bytes) const;
    void               forget_device(uint64_t device);

    bool covers(uint64_t device, uint64_t address, uint64_t bytes) const;
    bool region_admitted(uint64_t device, const VkStridedDeviceAddressRegionKHR &region) const;
    bool trace_admitted(uint64_t device, const VkStridedDeviceAddressRegionKHR *regions, uint32_t width,
                        uint32_t height, uint32_t depth) const;
    bool geometry_admitted(uint64_t device, const VkAccelerationStructureGeometryKHR &geometry,
                           const VkAccelerationStructureBuildRangeInfoKHR &range, uint32_t primitives) const;
    bool descriptor_admitted(uint64_t device, const VkDescriptorGetInfoEXT &info) const;
    bool address_info_admitted(uint64_t device, const VkDescriptorAddressInfoEXT *info) const;

    uint32_t buffers() const;
    uint32_t structures() const;
    uint32_t ranges() const;
    bool     invariants_hold() const;

private:
    struct Buffer {
        uint64_t device_;
        uint64_t bytes_;
    };

    struct Structure {
        uint64_t device_;
        uint64_t bytes_;
    };

    GpuAddressRegistry              registry_;
    KeyedSlots<uint64_t, Buffer>    buffers_;
    KeyedSlots<uint64_t, Structure> structures_;
    Limits                          limits_;
};

}

#endif
