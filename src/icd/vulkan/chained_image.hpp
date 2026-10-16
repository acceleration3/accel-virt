/* One swapchain's frames shown through its window's DXGI chain
 * (icd::WindowChains; docs/shared-surfaces.md, step 5) rather than read back
 * into guest memory and blitted through GDI: the chain's slot, opened on the
 * swapchain's device by the object id it is shared as (feature 053), and an
 * image over that memory described as the host made the slot's texture
 * (ExportableImageInfo), which is what has the two devices lay it out alike.
 *
 * The slot takes the swapchain's channel order with its rows top first, so a
 * present copies its image in as it is (PresentDevice::copy_into) and the
 * chain shows it. One slot is enough: a present returns once the chain has
 * read it, and the next frame is copied in only after. */
#ifndef AV_ICD_CHAINED_IMAGE_HPP
#define AV_ICD_CHAINED_IMAGE_HPP

#include "icd/common/chain_slots.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::icd {

class EntryLookup;
class SlotFormat;
struct ChainedImageProbe;

class ChainedImage {
public:
    /* `host_device` is the host's handle of `device`, which a slot's memory
     * is opened into. */
    ChainedImage(EntryLookup &entries, VkDevice device, uint64_t host_device, WindowChains &chains,
                 ClientTransport &wire);
    ~ChainedImage();
    ChainedImage(const ChainedImage &) = delete;
    ChainedImage &operator=(const ChainedImage &) = delete;

    /* `window`'s chain at `extent` on the adapter `luid`, for images of
     * `format`, with its slot opened on the device. False, with nothing held
     * and no chain left for the window, for a format no slot takes, a chain
     * that cannot be had, or a slot the device will not open or make an image
     * of; and for a second open. */
    [[nodiscard]] bool open(void *window, uint64_t luid, VkFormat format, VkExtent2D extent);
    bool               opened() const;
    /* The image a frame is copied into; null unless opened. */
    VkImage            slot() const;
    /* The slot's image destroyed and its memory released, the window's chain
     * left as it is: a retired swapchain's window is its successor's. */
    void               release();
    /* release(), and the window's chain closed. */
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

}  // namespace av::icd

#endif  // AV_ICD_CHAINED_IMAGE_HPP
