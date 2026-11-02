#ifndef AV_DISPLAY_MODE_TEXT_HPP
#define AV_DISPLAY_MODE_TEXT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DisplayModeSet;

class DisplayModeText {
public:
    explicit DisplayModeText(const char *text);
    DisplayModeText(const DisplayModeText &) = delete;
    DisplayModeText &operator=(const DisplayModeText &) = delete;

    bool empty() const;
    [[nodiscard]] Status read(DisplayModeSet &modes) const;

private:
    bool edge(const char *&at, uint32_t &value) const;

    const char *text_;
};

}

#endif
