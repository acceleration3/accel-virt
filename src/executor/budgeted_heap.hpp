#ifndef AV_BUDGETED_HEAP_HPP
#define AV_BUDGETED_HEAP_HPP

#include "core/platform/block_source.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/vram_account.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ByteLedger;

class BudgetedHeap final : public BlockSource {
public:
    static constexpr size_t header_bytes = 16u;

    BudgetedHeap();
    BudgetedHeap(const BudgetedHeap &) = delete;
    BudgetedHeap &operator=(const BudgetedHeap &) = delete;

    void bind(ByteLedger *ledger, uint64_t limit);

    void *allocate(size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;

    uint64_t live() const;
    uint64_t held() const;
    uint64_t limit() const;
    uint64_t refusals() const;

private:
    HostHeap    raw_;
    VramAccount account_;
    uint64_t    limit_;
    uint64_t    refusals_;
};

}

#endif
