/* One context's guest-backed memory (docs/protocol.md): the ring - its 32-byte
 * header and 2^order bytes of data, in whole pages - the fence page and the
 * reply slot, each one physically contiguous block the device maps as a guest
 * blob of one entry. Zeroed, with the ring's fresh header written, before
 * anything is sent. PASSIVE. */
#ifndef AV_CONTEXT_MEMORY_HPP
#define AV_CONTEXT_MEMORY_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ContextMemoryProbe;

class ContextMemory {
public:
    static constexpr size_t page_bytes = 4096u;

    explicit ContextMemory(Platform &platform);
    ~ContextMemory();
    ContextMemory(const ContextMemory &) = delete;
    ContextMemory &operator=(const ContextMemory &) = delete;

    /* invalid_parameter for an order the protocol does not allow; no_memory,
     * holding nothing, when the platform has none; invalid_state once holding. */
    [[nodiscard]] Status allocate(uint8_t order);
    void                 release();
    bool                 allocated() const;
    uint8_t              order() const;
    uint8_t             *ring() const;
    size_t               ring_bytes() const;
    uint64_t             ring_address() const;
    uint8_t             *fence_page() const;
    uint64_t             fence_address() const;
    uint8_t             *reply() const;
    uint64_t             reply_address() const;

    bool invariants_hold() const;

private:
    /* A zeroed page-multiple block, or null. */
    uint8_t *allocate_block(size_t bytes);

    Platform &platform_;
    uint8_t  *ring_;
    uint8_t  *fence_;
    uint8_t  *reply_;
    size_t    ring_bytes_;
    uint8_t   order_;

    friend struct ContextMemoryProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_MEMORY_HPP
