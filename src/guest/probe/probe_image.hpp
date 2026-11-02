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

    bool make(uint32_t extent, ProbeOutcome &outcome);

    VkImage     image() const;
    VkImageView view() const;

    bool invariants_hold() const;

private:
    bool choose_type(uint32_t allowed, uint32_t &type) const;
    bool make_view(ProbeOutcome &outcome);

    ProbeDevice   &device_;
    VkImage        image_;
    VkDeviceMemory memory_;
    VkImageView    view_;

    friend struct ProbeImageProbe;
};

}

#endif
