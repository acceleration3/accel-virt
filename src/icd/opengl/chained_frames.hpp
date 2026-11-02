#ifndef AV_GLICD_CHAINED_FRAMES_HPP
#define AV_GLICD_CHAINED_FRAMES_HPP

#include "icd/common/chain_slots.hpp"
#include "icd/common/window_chains.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class ChainedFrames {
public:
    static constexpr uint32_t slot_count = icd::WindowChains::max_slots;
    enum : uint32_t {
        gl_texture_2d = 0x0DE1u,
        gl_texture_tiling = 0x9580u, gl_linear_tiling = 0x9585u,
    };

    ChainedFrames();
    ChainedFrames(const ChainedFrames &) = delete;
    ChainedFrames &operator=(const ChainedFrames &) = delete;

    [[nodiscard]] bool open(ClientTransport &wire, icd::WindowChains &chains, void *window, uint64_t luid,
                            uint32_t width, uint32_t height, bool srgb);
    bool               opened() const;
    bool               fits(void *window, uint32_t width, uint32_t height) const;
    void               draw(ClientTransport &wire, uint32_t source, uint32_t slot);
    void               close(ClientTransport *wire);

    bool invariants_hold() const;

private:
    void delete_textures(ClientTransport &wire);

    icd::ChainSlots slots_;
    uint32_t        textures_[slot_count];
    bool            opened_;
};

}

#endif
