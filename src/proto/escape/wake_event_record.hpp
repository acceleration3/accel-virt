/* The escape record a process sends, once its av context exists, to have the
 * driver set an event of its own whenever the host raises on that context
 * (docs/protocol.md, "Wake event record"): the fence page's `raised` moving.
 * A thread waiting for the host to finish something then sleeps on the event
 * rather than asking again on a timer.
 *
 * The kernel decodes it from memory the process wrote and writes nothing back:
 * the answer is the escape's status. */
#ifndef AV_WAKE_EVENT_RECORD_HPP
#define AV_WAKE_EVENT_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WakeEventRecord {
public:
    /* As the other escape records: dxgkrnl hands a shorter private escape to
     * no driver. */
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00455641u;  /* "AVE" */

    WakeEventRecord();
    /* `event` is the process's handle to an event it may set. */
    explicit WakeEventRecord(uint64_t event);

    uint64_t event() const;
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a wrong length, magic, version or reserved field, or no
     * handle; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    uint64_t event_;
};

}  // namespace av

#endif  // AV_WAKE_EVENT_RECORD_HPP
