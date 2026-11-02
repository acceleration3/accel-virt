#ifndef AV_TRACE_ESCAPE_RECORD_HPP
#define AV_TRACE_ESCAPE_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class TraceEscapeRecord {
public:
    static constexpr size_t   encoded_size = 40u;
    static constexpr uint32_t magic = 0x00445641u;

    enum class Kind : uint16_t { table = 1u, state = 2u };

    TraceEscapeRecord();
    TraceEscapeRecord(Kind kind, uint64_t offset, uint32_t window);

    Kind     kind() const;
    uint64_t offset() const;
    uint32_t window() const;
    uint64_t total() const;
    uint64_t clock() const;
    TraceEscapeRecord answered(uint64_t total, uint32_t filled, uint64_t clock) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status accept_answer(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    [[nodiscard]] Status read(const uint8_t *bytes, size_t length);

    Kind     kind_;
    uint64_t offset_;
    uint32_t window_;
    uint64_t total_;
    uint64_t clock_;
};

}

#endif
