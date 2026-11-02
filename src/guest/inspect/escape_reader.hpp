/* The driver's trace table or state dump, read through D3DKMTEscape one
 * 16 KiB window at a time (docs/trace.md, TraceEscapeRecord) on the avgpu
 * adapter. The first answer says how much there is; every later one must say
 * the same and fill its window, or the read is refused - a dump that changes
 * under the reader is not one dump. */
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

    /* Reads the whole of `kind` from the avgpu adapter of `luid` - the first
     * for 0 - replacing what was held. not_found without that adapter; the escape's own failure; malformed for an answer
     * that is not one to the request, a total that changed or a window left
     * short; too_large past max_total; no_memory. Nothing is held unless ok. */
    [[nodiscard]] Status read(TraceEscapeRecord::Kind kind, uint64_t luid = 0u);

    const uint8_t *bytes() const;
    size_t         length() const;
    /* The driver's clock in its last answer. */
    uint64_t       clock() const;

    bool invariants_hold() const;

private:
    /* One window at `offset`, answered into `answer`. */
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

}  // namespace av

#endif  // AV_ESCAPE_READER_HPP
