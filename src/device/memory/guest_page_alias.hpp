#ifndef AV_GUEST_PAGE_ALIAS_HPP
#define AV_GUEST_PAGE_ALIAS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobWindow;
class Host;
struct GuestPageAliasProbe;

class GuestPageAlias {
public:
    static constexpr uint64_t page_size = 4096u;
    static constexpr uint32_t max_runs = 16384u;

    explicit GuestPageAlias(Host &host);
    ~GuestPageAlias();
    GuestPageAlias(const GuestPageAlias &) = delete;
    GuestPageAlias &operator=(const GuestPageAlias &) = delete;

    [[nodiscard]] Status map(const BlobWindow &pages);
    void                 release();
    bool                 mapped() const;
    void                *base() const;
    uint64_t             length() const;
    uint32_t             runs() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status map_runs(const BlobWindow &pages);

    Host    &host_;
    void    *base_;
    uint64_t length_;
    uint32_t runs_;

    friend struct GuestPageAliasProbe;
};

}

#endif
