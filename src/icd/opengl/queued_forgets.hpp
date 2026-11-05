#ifndef AV_GLICD_QUEUED_FORGETS_HPP
#define AV_GLICD_QUEUED_FORGETS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::glicd {

class QueuedForgets {
public:
    explicit QueuedForgets(BlockSource &memory);
    QueuedForgets(const QueuedForgets &) = delete;
    QueuedForgets &operator=(const QueuedForgets &) = delete;

    [[nodiscard]] Status add(uint32_t buffer);
    bool                 take(uint32_t &buffer);
    uint32_t             waiting() const;
    void                 clear();

    bool invariants_hold() const;

private:
    GrowableArray<uint32_t> buffers_;
    uint32_t                waiting_;
};

}

#endif
