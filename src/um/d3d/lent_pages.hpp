#ifndef AV_UM_LENT_PAGES_HPP
#define AV_UM_LENT_PAGES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ThreadLock;
}

namespace av::d3d {

class LentPages {
public:
    LentPages(BlockSource &memory, ThreadLock &lock);
    ~LentPages();
    LentPages(const LentPages &) = delete;
    LentPages &operator=(const LentPages &) = delete;

    [[nodiscard]] Status retire(void *pages, uint64_t fence);
    void                 reclaim(uint64_t completed);
    void                 release_all();

    bool invariants_hold() const;

private:
    struct Waiting {
        void    *pages_;
        uint64_t fence_;
    };

    GrowableArray<Waiting> waiting_;
    ThreadLock            &lock_;
};

}

#endif
