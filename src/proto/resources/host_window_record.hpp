/* The escape record a process sends to take a chunk of its device's host
 * window, or to give one back (docs/host-window.md).
 *
 * The kernel decodes it from memory the process wrote, so every field is
 * judged; it then writes its answer - the resource the chunk became, its size
 * in whole pages and where the process now sees it - back into the same
 * bytes. An answer is not a request: a process cannot pre-fill the fields the
 * kernel owns. */
#ifndef AV_HOST_WINDOW_RECORD_HPP
#define AV_HOST_WINDOW_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HostWindowRecord {
public:
    /* dxgkrnl hands a private escape this short to no driver; the size of
     * the other escape records does get through. */
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00575641u;  /* "AVW" */

    enum class Operation : uint16_t { open = 1u, close = 2u };

    HostWindowRecord();
    /* An open names the bytes wanted; a close names the resource. */
    HostWindowRecord(Operation operation, uint64_t bytes, uint32_t resource_id);

    bool     opens() const;
    uint32_t resource_id() const;
    uint64_t bytes() const;
    uint64_t address() const;
    /* This open with the kernel's answer filled in. */
    HostWindowRecord answered(uint32_t resource_id, uint64_t bytes, uint64_t address) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* A request only; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* The kernel's answer to this request, read back from the bytes the
     * process sent it in: for an open, a resource, at least the bytes asked
     * for and an address; a close answers itself. malformed for anything
     * else, and unchanged unless ok. */
    [[nodiscard]] Status accept_answer(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    bool is_request() const;
    /* Every field from the bytes, judging only what no record may carry. */
    [[nodiscard]] Status read(const uint8_t *bytes, size_t length);

    uint16_t operation_;
    uint32_t resource_id_;
    uint64_t bytes_;
    uint64_t address_;
};

}  // namespace av

#endif  // AV_HOST_WINDOW_RECORD_HPP
