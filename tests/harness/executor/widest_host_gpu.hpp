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

    const std::uint8_t *uuid() const;
    std::uint32_t       sub_apis() const;

private:
    std::uint8_t  uuid_[uuid_bytes];
    std::uint32_t sub_apis_;
};

}

#endif
