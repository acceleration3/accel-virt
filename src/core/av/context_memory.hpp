/* The memory one av context runs on (docs/protocol.md, "Context memory"):
 * its ring - the 32-byte header and 2^order bytes of data, in whole pages -
 * its fence page and its reply slot, in guest RAM the driver alone owns, so the
 * pages the device is handed stay where they are whatever VidMm does with the
 * process's allocations.
 *
 * Besides the frames of every page it holds the kernel's own views of the
 * ring header and of the fence page - the only bytes the driver itself touches
 * - and, once mapped, the process's views of the ring and of the reply slot.
 * The ring's fresh header is written before anything is sent. PASSIVE. */
#ifndef AV_CONTEXT_MEMORY_HPP
#define AV_CONTEXT_MEMORY_HPP

#include "core/memory/guest_pages.hpp"
#include "core/platform/platform.hpp"
#include "core/av/context_pages.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct ContextMemoryProbe;

class ContextMemory {
public:
    explicit ContextMemory(Platform &platform);
    ~ContextMemory();
    ContextMemory(const ContextMemory &) = delete;
    ContextMemory &operator=(const ContextMemory &) = delete;

    /* A ring of `order`, a fence page and a reply slot of `reply_bytes` in
     * whole pages, zeroed, with the ring's header written and the kernel's two
     * views mapped. invalid_parameter for an order or a reply size the protocol
     * does not allow; invalid_state once holding; no_memory, holding nothing,
     * when the platform has no memory or cannot map a view. */
    [[nodiscard]] Status allocate(uint8_t order, uint32_t reply_bytes);
    /* The ring and the reply slot mapped into the calling process.
     * invalid_state unless allocated and not yet mapped; the platform's
     * refusal otherwise, with neither mapped. */
    [[nodiscard]] Status map_into_process();
    /* Every view gone, then the pages. */
    void                 release();
    /* Every view gone and the pages kept out of use for good: a device that
     * never answered the context's teardown may still write them. */
    void                 abandon();

    bool         allocated() const;
    uint8_t      order() const;
    /* The process's addresses of the ring and of the reply slot; 0 unless
     * mapped. */
    uint64_t     ring_view() const;
    uint64_t     reply_view() const;
    /* Empty unless allocated; valid until released or abandoned. */
    ContextPages pages() const;

    bool invariants_hold() const;

private:
    /* The kernel's two views gone, if mapped. */
    void unmap_kernel_views();
    /* Every view gone, then the pages, of whatever part is held. */
    void free_all();

    Platform  &platform_;
    GuestPages ring_;
    GuestPages fence_;
    GuestPages reply_;
    uint8_t   *ring_header_;
    uint8_t   *fence_view_;
    uint8_t    order_;

    friend struct ContextMemoryProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_MEMORY_HPP
