#ifndef AV_SCRIPTED_WINDOW_CHAINS_HPP
#define AV_SCRIPTED_WINDOW_CHAINS_HPP

#include "icd/common/window_chains.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowChains final : public icd::WindowChains {
public:
    static constexpr std::uint64_t first_id = 0x51A7000ull;

    ScriptedWindowChains();

    void refuse_opens(bool refuse);
    void refuse_presents(bool refuse);

    bool open(void *window, std::uint64_t luid, std::uint32_t width, std::uint32_t height, icd::SlotLayout layout,
              std::uint32_t slots, std::uint64_t *ids, std::uint64_t *bytes) override;
    bool present(void *window, std::uint32_t slot) override;
    void close(void *window) override;

    std::uint32_t opens() const;
    std::uint32_t presents() const;
    std::uint32_t closes() const;
    void         *open_window() const;
    std::uint64_t last_luid() const;
    std::uint32_t last_width() const;
    std::uint32_t last_height() const;
    icd::SlotLayout last_layout() const;
    std::uint32_t last_slots() const;
    std::uint32_t last_slot() const;
    void         *last_closed() const;

private:
    bool          refuse_opens_;
    bool          refuse_presents_;
    std::uint32_t opens_;
    std::uint32_t presents_;
    std::uint32_t closes_;
    void         *window_;
    std::uint64_t luid_;
    std::uint32_t width_;
    std::uint32_t height_;
    icd::SlotLayout layout_;
    std::uint32_t slots_;
    std::uint32_t slot_;
    void         *closed_;
};

}

#endif
