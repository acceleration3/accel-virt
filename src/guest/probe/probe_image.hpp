/* The probe's render target (feature 045, T037): a square R8G8B8A8_UNORM
 * image drawn into and copied out, in device-local memory where the image may
 * use it and any type it may otherwise, with a view for its framebuffer.
 * Destroyed with its memory when this goes. */
#ifndef AV_PROBE_IMAGE_HPP
#define AV_PROBE_IMAGE_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ProbeDevice;
class ProbeOutcome;
struct ProbeImageProbe;

class ProbeImage {
public:
    explicit ProbeImage(ProbeDevice &device);
    ~ProbeImage();
    ProbeImage(const ProbeImage &) = delete;
    ProbeImage &operator=(const ProbeImage &) = delete;

    /* `extent` pixels on a side: whether it has its view, with `outcome` told
     * why not. */
    bool make(uint32_t extent, ProbeOutcome &outcome);

    VkImage     image() const;
    VkImageView view() const;

    /* A view only of a bound image, bound only to memory of its own. */
    bool invariants_hold() const;

private:
    /* Device-local if it may be, else the lowest type it may use. */
    bool choose_type(uint32_t allowed, uint32_t &type) const;
    bool make_view(ProbeOutcome &outcome);

    ProbeDevice   &device_;
    VkImage        image_;
    VkDeviceMemory memory_;
    VkImageView    view_;

    friend struct ProbeImageProbe;
};

}  // namespace av

#endif  // AV_PROBE_IMAGE_HPP
