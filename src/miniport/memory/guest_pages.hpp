#ifndef AV_GUEST_PAGES_HPP
#define AV_GUEST_PAGES_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <stdint.h>

namespace av {

struct GuestPagesProbe;

class GuestPages {
public:
    explicit GuestPages(Platform &platform);
    ~GuestPages();
    GuestPages(const GuestPages &) = delete;
    GuestPages &operator=(const GuestPages &) = delete;

    [[nodiscard]] Status allocate(uint64_t bytes);
    [[nodiscard]] Status map_into_process();
    [[nodiscard]] Status map_read_only_into_process();
    void                 unmap_from_process();
    void                 release();
    void                 abandon();

    bool          held() const;
    uint64_t      bytes() const;
    GuestPageList pages() const;
    uint64_t      view() const;

    bool invariants_hold() const;

private:
    Platform &platform_;
    void     *record_;
    uint64_t *frames_;
    uint64_t  bytes_;
    void     *mapping_;
    uint64_t  view_;

    friend struct GuestPagesProbe;
};

}

#endif
