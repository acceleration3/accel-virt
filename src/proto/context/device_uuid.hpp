#ifndef AV_DEVICE_UUID_HPP
#define AV_DEVICE_UUID_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class TextBuffer;
class WireReader;
class WireWriter;

class DeviceUuid {
public:
    static constexpr uint32_t bytes = 16u;

    DeviceUuid();
    explicit DeviceUuid(const uint8_t *uuid);

    [[nodiscard]] Status parse(const char *text);
    bool                 equals(const uint8_t *uuid) const;
    bool                 equals(const DeviceUuid &other) const;
    void                 append_hex(TextBuffer &text) const;
    const uint8_t       *raw() const;
    void                 encode(WireWriter &writer) const;
    void                 decode(WireReader &reader);

private:
    static constexpr uint32_t text_length = 36u;

    int32_t digit(char c) const;
    bool    separator_at(uint32_t position) const;

    uint8_t bytes_[bytes];
};

}

#endif
