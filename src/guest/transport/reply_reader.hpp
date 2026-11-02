#ifndef AV_REPLY_READER_HPP
#define AV_REPLY_READER_HPP

#include "core/support/status.hpp"
#include "proto/submission/reply_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
struct ReplyReaderProbe;

class ReplyReader {
public:
    ReplyReader();
    ReplyReader(const uint8_t *slot, size_t capacity);

    [[nodiscard]] Status read(uint64_t expected_fence, WireReader &answer);
    uint32_t required() const;

    bool invariants_hold() const;

private:
    const uint8_t *slot_;
    size_t         capacity_;
    uint32_t       required_;

    friend struct ReplyReaderProbe;
};

}

#endif
