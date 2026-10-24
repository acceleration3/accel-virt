/* The escape a process sends around each read it makes of an allocation's
 * pages that presents write - DWM's user-mode driver refreshing its texture of
 * a window's GDI surface from them (docs/protocol.md, "Pages read record";
 * docs/stale-blt-frames.md). Presents into the pages are the present engine's,
 * moved on a host thread after the present returned, and nothing in the
 * kernel orders a CPU read after them. So the reader says where its read
 * begins and ends: at the begin the driver waits until every present queued
 * into the pages has moved and answers the allocation's write sequence; at the
 * end it answers the sequence again, and a reader whose two differ read while
 * a present was being written, and does not show what it read.
 *
 * The kernel decodes the request from memory the process wrote, and writes
 * the answer back into the same bytes. */
#ifndef AV_PAGES_READ_RECORD_HPP
#define AV_PAGES_READ_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PagesReadRecord {
public:
    /* As the other escape records: dxgkrnl hands a shorter private escape to
     * no driver. */
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00425641u;  /* "AVB" */

    enum class Action : uint16_t { begin = 1u, end = 2u };

    PagesReadRecord();
    /* `allocation` is the process's D3DKMT_HANDLE of the allocation whose pages
     * it reads; `wait_microseconds` how long a begin may wait for the presents
     * into them, 0 for an end. */
    PagesReadRecord(Action action, uint32_t allocation, uint32_t wait_microseconds);

    Action   action() const;
    uint32_t allocation() const;
    uint32_t wait_microseconds() const;
    /* The answer: the allocation's write sequence, and whether every present
     * queued into its pages had moved when the read began (an end always
     * says so). */
    uint64_t writes() const;
    bool     complete() const;
    PagesReadRecord answered(uint64_t writes, bool complete) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* The request as the kernel reads it: malformed for a wrong length,
     * magic, version or reserved field, an action that is neither, no
     * allocation, or an answer already filled in; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* The answer as the process reads it back: as decode, but the answer's
     * fields may hold anything. */
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

}  // namespace av

#endif  // AV_PAGES_READ_RECORD_HPP
