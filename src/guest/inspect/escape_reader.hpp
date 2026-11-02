#ifndef AV_ESCAPE_READER_HPP
#define AV_ESCAPE_READER_HPP

#include "core/support/status.hpp"
#include "proto/escape/trace_escape_record.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class Kmt;
struct EscapeReaderProbe;

class EscapeReader {
public:
    static constexpr uint32_t window_bytes = 16u * 1024u;
    static constexpr uint64_t max_total = 64u * 1024u * 1024u;

    EscapeReader(Kmt &kmt, BlockSource &memory);
    ~EscapeReader();
    EscapeReader(const EscapeReader &) = delete;
    EscapeReader &operator=(const EscapeReader &) = delete;

    [[nodiscard]] Status read(TraceEscapeRecord::Kind kind);

    const uint8_t *bytes() const;
    size_t         length() const;
    uint64_t       clock() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status page(uint32_t adapter, TraceEscapeRecord::Kind kind, uint64_t offset,
                              TraceEscapeRecord &answer);
    [[nodiscard]] Status read_from(uint32_t adapter, TraceEscapeRecord::Kind kind);
    void                 drop();

    Kmt         &kmt_;
    BlockSource &memory_;
    uint8_t     *bytes_;
    size_t       length_;
    uint64_t     clock_;
    uint8_t      page_[TraceEscapeRecord::encoded_size + window_bytes];

    friend struct EscapeReaderProbe;
};

}

#endif
