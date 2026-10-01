/* The GPU this host's own loaders say runs the most sub-APIs, the first of
 * them on a tie: the one a device given the real executor library is bound to
 * in L4 (feature 044, T028), found from the machine rather than a UUID written
 * into a test. A host with no GPU finds none, and says so as zero sub-APIs. */
#ifndef AV_WIDEST_HOST_GPU_HPP
#define AV_WIDEST_HOST_GPU_HPP

#include <cstdint>

namespace av {

class WidestHostGpu {
public:
    static constexpr std::uint32_t uuid_bytes = 16u;

    WidestHostGpu();
    WidestHostGpu(const WidestHostGpu &) = delete;
    WidestHostGpu &operator=(const WidestHostGpu &) = delete;

    /* Its Vulkan device UUID; all zero when there is none. */
    const std::uint8_t *uuid() const;
    /* The AV_EXECUTOR_SUB_API_* bits it runs. */
    std::uint32_t       sub_apis() const;

private:
    std::uint8_t  uuid_[uuid_bytes];
    std::uint32_t sub_apis_;
};

}  // namespace av

#endif  // AV_WIDEST_HOST_GPU_HPP
