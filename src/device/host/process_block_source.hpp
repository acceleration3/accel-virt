/* Blocks from the process's own heap, for what outlives any one device: the
 * guest RAM lendings the devices of one QEMU process share (GuestRamLendings)
 * must go on when the device that made one is gone, so they cannot keep their
 * memory in that device's host. */
#ifndef AV_PROCESS_BLOCK_SOURCE_HPP
#define AV_PROCESS_BLOCK_SOURCE_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>

namespace av {

class ProcessBlockSource final : public BlockSource {
public:
    ProcessBlockSource();

    void  *allocate(size_t bytes, Pool pool) override;
    void   release(void *block) override;
    /* Any thread may use the process's heap. */
    void   require_irql_at_most(Irql ceiling, const char *function) override;
    /* Blocks given and not yet taken back. */
    size_t live() const;

private:
    size_t blocks_;
};

}  // namespace av

#endif  // AV_PROCESS_BLOCK_SOURCE_HPP
