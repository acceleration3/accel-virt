#ifndef AV_WINDOW_FRAME_HPP
#define AV_WINDOW_FRAME_HPP

#include <stdint.h>

namespace av {

class WindowFrame {
public:
    WindowFrame(const WindowFrame &) = delete;
    WindowFrame &operator=(const WindowFrame &) = delete;

    virtual bool size(void *window, uint32_t &width, uint32_t &height) = 0;
    virtual bool read(void *window, uint32_t width, uint32_t height, uint8_t *bgra) = 0;
    virtual bool resize(void *window, uint32_t width, uint32_t height) = 0;

protected:
    WindowFrame() = default;
    ~WindowFrame() = default;
};

}

#endif
