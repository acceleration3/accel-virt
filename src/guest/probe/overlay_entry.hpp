#ifndef AV_OVERLAY_ENTRY_HPP
#define AV_OVERLAY_ENTRY_HPP

#include <stdint.h>

namespace av {

class OverlayEntry {
public:
    OverlayEntry(const OverlayEntry &) = delete;
    OverlayEntry &operator=(const OverlayEntry &) = delete;

    virtual int32_t create_device() = 0;
    virtual int32_t supports_overlays(bool &supported) = 0;
    virtual int32_t overlay_support(uint32_t format, uint32_t &flags) = 0;
    virtual int32_t create_swap_chain(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;
    virtual int32_t present(const uint8_t *colour) = 0;
    virtual int32_t composition(uint32_t &mode) = 0;
    virtual void    release() = 0;

protected:
    OverlayEntry() = default;
    ~OverlayEntry() = default;
};

}

#endif
