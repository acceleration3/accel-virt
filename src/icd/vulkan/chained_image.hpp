#ifndef AV_ICD_CHAINED_IMAGE_HPP
#define AV_ICD_CHAINED_IMAGE_HPP

#include "icd/common/chain_slots.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class EntryLookup;
class SlotFormat;
struct ChainedImageProbe;

class ChainedImage {
public:
    ChainedImage(EntryLookup &entries, VkDevice device, uint64_t host_device, WindowChains &chains,
                 ClientTransport &wire);
    ~ChainedImage();
    ChainedImage(const ChainedImage &) = delete;
    ChainedImage &operator=(const ChainedImage &) = delete;

    [[nodiscard]] bool open(void *window, uint64_t luid, VkFormat format, VkExtent2D extent);
    bool               opened() const;
    VkImage            slot() const;
    void               release();
    void               close();

    bool invariants_hold() const;

private:
    [[nodiscard]] bool make_image(const SlotFormat &slot, VkExtent2D extent);

    EntryLookup     &entries_;
    VkDevice         device_;
    uint64_t         host_device_;
    WindowChains    &chains_;
    ChainSlots       slots_;
    ClientTransport &wire_;
    VkImage          image_;

    friend struct ChainedImageProbe;
};

}

#endif
