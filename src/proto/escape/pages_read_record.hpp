#ifndef AV_PAGES_READ_RECORD_HPP
#define AV_PAGES_READ_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PagesReadRecord {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00425641u;

    enum class Action : uint16_t { begin = 1u, end = 2u };

    PagesReadRecord();
    PagesReadRecord(Action action, uint32_t allocation, uint32_t wait_microseconds);

    Action   action() const;
    uint32_t allocation() const;
    uint32_t wait_microseconds() const;
    uint64_t writes() const;
    bool     complete() const;
    PagesReadRecord answered(uint64_t writes, bool complete) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status read_answer(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    [[nodiscard]] Status read(const uint8_t *bytes, size_t length, bool answered);

    Action   action_;
    uint32_t allocation_;
    uint32_t wait_microseconds_;
    uint64_t writes_;
    bool     complete_;
};

}

#endif
