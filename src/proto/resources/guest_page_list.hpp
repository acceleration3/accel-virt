/* The guest pages backing one resource, in the order the device reads them: a
 * view over page frame addresses that knows how they group into runs of
 * physically adjacent pages, so RESOURCE_ATTACH_BACKING carries one memory
 * entry per run (virtio 1.2 5.7.6.3) instead of one per page. The frames
 * belong to the caller and must outlive the list. */
#ifndef AV_GUEST_PAGE_LIST_HPP
#define AV_GUEST_PAGE_LIST_HPP

#include <stdint.h>

namespace av {

class GuestPageList {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    GuestPageList();
    GuestPageList(const uint64_t *frames, uint32_t count);

    uint32_t count() const;
    /* Zero past the end. */
    uint64_t frame(uint32_t index) const;
    /* Every page has a page-aligned address; an empty list is not complete. */
    bool     complete() const;
    /* Pages adjacent to and including `index`; zero past the end. */
    uint32_t run_length(uint32_t index) const;
    /* Maximal runs of adjacent pages; zero for an empty list. */
    uint32_t runs() const;

private:
    const uint64_t *frames_;
    uint32_t        count_;
};

}  // namespace av

#endif  // AV_GUEST_PAGE_LIST_HPP
