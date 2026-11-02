#ifndef AV_WINDOW_CURSOR_HPP
#define AV_WINDOW_CURSOR_HPP

#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowCursor {
public:
    explicit WindowCursor(const BlobWindow &window);

    [[nodiscard]] Status read(uint64_t offset, uint8_t *bytes, size_t count);
    [[nodiscard]] Status write(uint64_t offset, const uint8_t *bytes, size_t count);

private:
    uint8_t *at(uint64_t offset, uint64_t &bytes);
    bool     contains(uint64_t offset, size_t count) const;

    BlobWindow window_;
    uint32_t   extent_;
    uint64_t   start_;
};

}

#endif
