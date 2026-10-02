/* A buffer of the probe's in memory the process maps (feature 045, T037): the
 * lowest host-visible type the buffer may use, allocated, bound and mapped
 * whole. The mapping is the memory itself, so what the GPU wrote is read
 * straight from it. Destroyed with its memory when this goes. */
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

    /* `bytes` of `usage`: whether it is mapped, with `outcome` told why not. */
    bool make(VkDeviceSize bytes, VkBufferUsageFlags usage, ProbeOutcome &outcome);

    VkBuffer       buffer() const;
    /* The mapping; null until made. */
    unsigned char *bytes() const;

    /* Mapped only when bound, and bound only to memory of its own. */
    bool invariants_hold() const;

private:
    ProbeDevice   &device_;
    VkBuffer       buffer_;
    VkDeviceMemory memory_;
    unsigned char *bytes_;

    friend struct ProbeBufferProbe;
};

}  // namespace av

#endif  // AV_PROBE_BUFFER_HPP
