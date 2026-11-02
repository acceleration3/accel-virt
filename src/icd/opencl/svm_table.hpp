#ifndef AV_CLICD_SVM_TABLE_HPP
#define AV_CLICD_SVM_TABLE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "icd/opencl/svm_allocation.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::clicd {

struct SvmTableProbe;

class SvmTable {
public:
    explicit SvmTable(BlockSource &memory);
    ~SvmTable();
    SvmTable(const SvmTable &) = delete;
    SvmTable &operator=(const SvmTable &) = delete;

    void *take(uint64_t host_base, size_t bytes);
    uint64_t give_back(const void *mirror);
    void     give_back_all();

    uint64_t host_address_of(const void *address) const;
    SvmAllocation       *covering(const void *address);
    const SvmAllocation *covering(const void *address) const;

    uint32_t live() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct SvmTableProbe;

    uint64_t key_of(const void *address) const;
    uint32_t row_below(uint64_t key) const;

    BlockSource                         &memory_;
    KeyedSlots<uint64_t, SvmAllocation>  allocations_;
    GrowableArray<uint64_t>              bases_;
};

}

#endif
