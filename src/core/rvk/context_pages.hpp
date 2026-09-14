/* The memory one rvk context runs on, as each of its two readers reaches it
 * (docs/protocol.md): the guest pages of the ring, fence page and reply slot,
 * which the device is handed as guest blobs, and the kernel's view of the ring
 * header and of the fence page, the only bytes the driver itself touches. In a
 * guest those are a process's aperture allocations; the frames and the views
 * belong to whoever built this and must outlive every copy of it. */
#ifndef AV_CONTEXT_PAGES_HPP
#define AV_CONTEXT_PAGES_HPP

#include "proto/resources/guest_page_list.hpp"

#include <stdint.h>

namespace av {

class ContextPages {
public:
    ContextPages();
    ContextPages(const GuestPageList &ring, const GuestPageList &fence, const GuestPageList &reply,
                 uint8_t *ring_view, const uint8_t *fence_view);

    GuestPageList  ring() const;
    GuestPageList  fence() const;
    GuestPageList  reply() const;
    /* Writable: the driver publishes the tail into the ring's header. */
    uint8_t       *ring_view() const;
    const uint8_t *fence_view() const;
    /* Every list complete, the fence page exactly one page, and both views
     * present: memory a context can be set up on. */
    bool           complete() const;

private:
    GuestPageList  ring_;
    GuestPageList  fence_;
    GuestPageList  reply_;
    uint8_t       *ring_view_;
    const uint8_t *fence_view_;
};

}  // namespace av

#endif  // AV_CONTEXT_PAGES_HPP
