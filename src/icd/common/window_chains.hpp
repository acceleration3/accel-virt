#ifndef AV_ICD_WINDOW_CHAINS_HPP
#define AV_ICD_WINDOW_CHAINS_HPP

#include <stdint.h>

namespace av::icd {
enum class SlotLayout : uint32_t {
    rgba_bottom_up,
    rgba_top_down,
    bgra_top_down,
};

class WindowChains {
public:
    static constexpr uint32_t max_slots = 2u;

    WindowChains(const WindowChains &) = delete;
    WindowChains &operator=(const WindowChains &) = delete;

    virtual bool open(void *window, uint64_t luid, uint32_t width, uint32_t height, SlotLayout layout,
                      uint32_t slots, uint64_t *ids, uint64_t *bytes) = 0;
    virtual bool present(void *window, uint32_t slot) = 0;
    virtual void close(void *window) = 0;

protected:
    WindowChains() = default;
    ~WindowChains() = default;
};

}

#endif
