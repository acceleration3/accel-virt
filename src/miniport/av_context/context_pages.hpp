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
    uint8_t       *ring_view() const;
    const uint8_t *fence_view() const;
    bool           complete() const;

private:
    GuestPageList  ring_;
    GuestPageList  fence_;
    GuestPageList  reply_;
    uint8_t       *ring_view_;
    const uint8_t *fence_view_;
};

}

#endif
