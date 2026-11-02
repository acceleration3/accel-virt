#ifndef AV_SCOPE_NAME_HPP
#define AV_SCOPE_NAME_HPP

#include <stdint.h>

namespace av {

class ScopeName {
public:
    constexpr explicit ScopeName(const char *text) : text_(text), id_(fnv_offset)
    {
        for (const char *cursor = text; cursor != nullptr && *cursor != '\0'; ++cursor)
            id_ = (id_ ^ static_cast<uint8_t>(*cursor)) * fnv_prime;
    }

    constexpr const char *text() const { return text_; }
    constexpr uint32_t    id() const { return id_; }

private:
    static constexpr uint32_t fnv_offset = 2166136261u;
    static constexpr uint32_t fnv_prime = 16777619u;

    const char *text_;
    uint32_t    id_;
};

}

#endif
