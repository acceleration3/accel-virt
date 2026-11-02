#ifndef AV_SWAP_CHAIN_BUFFER_HPP
#define AV_SWAP_CHAIN_BUFFER_HPP

#include <stdint.h>

namespace av::d3d {

class SwapChainBuffer {
public:
    enum class Model : uint8_t { blt, flip, scanout };

    static constexpr uint32_t misc_shared = 0x2u;

    SwapChainBuffer(bool primary, uint32_t misc);

    Model model() const;

private:
    bool     primary_;
    uint32_t misc_;
};

}

#endif
