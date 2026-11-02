#ifndef AV_PROBE_BUFFER_HPP
#define AV_PROBE_BUFFER_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ProbeDevice;
class ProbeOutcome;
struct ProbeBufferProbe;

class ProbeBuffer {
public:
    explicit ProbeBuffer(ProbeDevice &device);
    ~ProbeBuffer();
    ProbeBuffer(const ProbeBuffer &) = delete;
    ProbeBuffer &operator=(const ProbeBuffer &) = delete;

    bool make(VkDeviceSize bytes, VkBufferUsageFlags usage, ProbeOutcome &outcome);

    VkBuffer       buffer() const;
    unsigned char *bytes() const;

    bool invariants_hold() const;

private:
    ProbeDevice   &device_;
    VkBuffer       buffer_;
    VkDeviceMemory memory_;
    unsigned char *bytes_;

    friend struct ProbeBufferProbe;
};

}

#endif
