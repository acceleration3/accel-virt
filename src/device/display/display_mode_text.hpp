/* The modes a device is told to offer, as its `modes` property writes them:
 * WxH entries between commas, the first preferred - "2560x1440,1920x1080,80x80".
 * Any size a display could have, standard or not. Spaces around an entry are
 * allowed; anything else refuses the whole text, so a typo never becomes a
 * list with one mode missing. */
#ifndef AV_DISPLAY_MODE_TEXT_HPP
#define AV_DISPLAY_MODE_TEXT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DisplayModeSet;

class DisplayModeText {
public:
    /* `text` outlives the object; null is no text. */
    explicit DisplayModeText(const char *text);
    DisplayModeText(const DisplayModeText &) = delete;
    DisplayModeText &operator=(const DisplayModeText &) = delete;

    /* Null, or nothing but spaces: told no modes. */
    bool empty() const;
    /* The entries into `modes`, the first preferred. invalid_parameter for an
     * entry that is not WxH, an edge of 0 or past the set's bound, too many
     * entries, or no entry; `modes` is then as it was. */
    [[nodiscard]] Status read(DisplayModeSet &modes) const;

private:
    /* One decimal edge at `at`, moved past it; false for none or one too long. */
    bool edge(const char *&at, uint32_t &value) const;

    const char *text_;
};

}  // namespace av

#endif  // AV_DISPLAY_MODE_TEXT_HPP
