#ifndef AV_NEXT_POINT_RECORD_HPP
#define AV_NEXT_POINT_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class NextPointRecord {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t magic = 0x004E5641u;
    static constexpr uint32_t withdraw_magic = 0x004A5641u;
    static constexpr uint32_t pages_magic = 0x00555641u;

    enum class Action : uint8_t { name, withdraw, pages };

    NextPointRecord();
    NextPointRecord(uint64_t surface, uint32_t point, Action action = Action::name);

    uint64_t surface() const;
    uint32_t point() const;
    Action   action() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint64_t surface_;
    uint32_t point_;
    Action   action_;
};

}

#endif
