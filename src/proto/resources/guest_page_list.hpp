#ifndef AV_GUEST_PAGE_LIST_HPP
#define AV_GUEST_PAGE_LIST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireWriter;

class GuestPageList {
public:
    static constexpr uint64_t page_bytes = 0x1000u;
    static constexpr size_t   run_entry_bytes = 16u;

    GuestPageList();
    GuestPageList(const uint64_t *frames, uint32_t count);

    uint32_t count() const;
    uint64_t frame(uint32_t index) const;
    bool     complete() const;
    uint32_t run_length(uint32_t index) const;
    uint32_t runs() const;
    void     write_runs(WireWriter &writer) const;
    uint64_t fingerprint() const;

private:
    const uint64_t *frames_;
    uint32_t        count_;
};

}

#endif
