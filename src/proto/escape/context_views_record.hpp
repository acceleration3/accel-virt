#ifndef AV_CONTEXT_VIEWS_RECORD_HPP
#define AV_CONTEXT_VIEWS_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextViewsRecord {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00565641u;

    static constexpr uint16_t without_fence = 1u;
    static constexpr uint16_t with_fence    = 2u;

    ContextViewsRecord();
    explicit ContextViewsRecord(uint16_t version);
    uint16_t version() const;
    uint64_t ring() const;
    uint64_t reply() const;
    uint64_t fence() const;
    ContextViewsRecord answered(uint64_t ring, uint64_t reply, uint64_t fence) const;
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status accept_answer(const uint8_t *bytes, size_t length);

private:
    [[nodiscard]] Status read(const uint8_t *bytes, size_t length);

    uint16_t version_;
    uint64_t ring_;
    uint64_t reply_;
    uint64_t fence_;
};

}

#endif
