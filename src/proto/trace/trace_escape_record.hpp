/* The escape avinspect.exe sends the driver for its trace table or its state
 * dump, one window at a time (docs/trace.md). The request names the kind, the
 * offset and how many payload bytes follow the record; the driver answers in
 * the same bytes with the total size, its clock, and how many bytes of the
 * window it filled. Decoded by the kernel from memory a process wrote, and by
 * the process from memory the kernel wrote, so both judge every field. */
#ifndef AV_TRACE_ESCAPE_RECORD_HPP
#define AV_TRACE_ESCAPE_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class TraceEscapeRecord {
public:
    static constexpr size_t   encoded_size = 40u;
    static constexpr uint32_t magic = 0x00445641u; /* "AVD" */

    enum class Kind : uint16_t { table = 1u, state = 2u };

    TraceEscapeRecord();
    TraceEscapeRecord(Kind kind, uint64_t offset, uint32_t window);

    Kind     kind() const;
    uint64_t offset() const;
    uint32_t window() const;
    uint64_t total() const;
    uint64_t clock() const;
    /* This request answered: `total` bytes exist, `filled` of them were
     * written at the offset, at the driver's `clock`. */
    TraceEscapeRecord answered(uint64_t total, uint32_t filled, uint64_t clock) const;

    /* invalid_parameter, writing nothing, without room for the record. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* A request whose window fits the `length` bytes it arrived in, past the
     * record; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* The driver's answer to this request, read back from the bytes it was
     * sent in: the same kind, offset and a window no larger, with the filled
     * bytes inside the total. malformed otherwise; unchanged unless ok. */
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

}  // namespace av

#endif  // AV_TRACE_ESCAPE_RECORD_HPP
