#ifndef AV_HOST_COPY_IMAGES_HPP
#define AV_HOST_COPY_IMAGES_HPP

#include "core/support/keyed_slots.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class BlockSource;

class HostCopyImages {
public:
    static constexpr uint64_t most_bytes = uint64_t{1} << 30u;

    explicit HostCopyImages(BlockSource &blocks);
    HostCopyImages(const HostCopyImages &) = delete;
    HostCopyImages &operator=(const HostCopyImages &) = delete;

    [[nodiscard]] bool record(uint64_t image, uint64_t device, const VkImageCreateInfo &create);
    void               forget(uint64_t image);
    void               forget_device(uint64_t device);
    [[nodiscard]] bool bytes(uint64_t image, const VkImageSubresourceLayers &layers, VkExtent3D extent,
                             uint32_t row_length, uint32_t image_height, uint64_t &out) const;
    uint32_t           live() const;
    bool               invariants_hold() const;

private:
    struct Entry {
        uint64_t device_;
        VkFormat format_;
        uint32_t array_layers_;
    };

    KeyedSlots<uint64_t, Entry> slots_;
};

}

#endif
