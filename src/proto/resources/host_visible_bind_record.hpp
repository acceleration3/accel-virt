/* The escape record a process sends to bind one of its host-visible chunks to
 * its rvk context, or to unbind one (feature 045, contracts/host_visible_memory.md).
 *
 * The kernel decodes it from memory the process wrote, so every field is judged;
 * the kernel then writes its answer - the resource the chunk became and its page
 * count - back into the same bytes. An answer is not a request: a process cannot
 * pre-fill the fields the kernel owns. */
#ifndef AV_HOST_VISIBLE_BIND_RECORD_HPP
#define AV_HOST_VISIBLE_BIND_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HostVisibleBindRecord {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x484B5652u;  /* "RVKH" */

    enum class Operation : uint16_t { bind = 1u, unbind = 2u };

    HostVisibleBindRecord();
    /* A bind names the allocation and a zero resource; an unbind names the
     * resource. */
    HostVisibleBindRecord(Operation operation, uint32_t allocation, uint32_t resource_id);

    bool     binds() const;
    uint32_t allocation() const;
    uint32_t resource_id() const;
    uint64_t pages() const;
    /* This request with the kernel's answer filled in. */
    HostVisibleBindRecord answered(uint32_t resource_id, uint64_t pages) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* A request only; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    bool is_request() const;

    uint16_t operation_;
    uint32_t allocation_;
    uint32_t resource_id_;
    uint64_t pages_;
};

}  // namespace av

#endif  // AV_HOST_VISIBLE_BIND_RECORD_HPP
