#ifndef AV_ICD_CHAIN_SLOTS_HPP
#define AV_ICD_CHAIN_SLOTS_HPP

#include "icd/common/window_chains.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class ChainSlots {
public:
    static constexpr uint32_t max_slots = WindowChains::max_slots;

    ChainSlots();
    ChainSlots(const ChainSlots &) = delete;
    ChainSlots &operator=(const ChainSlots &) = delete;

    [[nodiscard]] bool open(ClientTransport &wire, WindowChains &chains, void *window, uint64_t luid, uint32_t width,
                            uint32_t height, SlotLayout layout, uint32_t count, uint64_t into);
    bool               opened() const;
    bool               fits(void *window, uint32_t width, uint32_t height) const;
    uint32_t           count() const;
    uint32_t           width() const;
    uint32_t           height() const;
    uint64_t           memory(uint32_t slot) const;
    [[nodiscard]] bool present(uint32_t slot);
    void               release(ClientTransport &wire);
    void               close();

    bool invariants_hold() const;

private:
    void forget_slots();

    WindowChains *chains_;
    void         *window_;
    uint32_t      width_;
    uint32_t      height_;
    uint32_t      count_;
    uint64_t      ids_[max_slots];
    uint64_t      bytes_[max_slots];
    uint64_t      memories_[max_slots];
    bool          opened_;
};

}

#endif
