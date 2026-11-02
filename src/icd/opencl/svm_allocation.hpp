#ifndef AV_CLICD_SVM_ALLOCATION_HPP
#define AV_CLICD_SVM_ALLOCATION_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct SvmAllocationProbe;

class SvmAllocation {
public:
    SvmAllocation();

    void take(uint64_t host_base, uint8_t *mirror, size_t bytes);
    void give_up();
    bool held() const;

    uint64_t host_base() const;
    uint8_t *mirror() const;
    size_t   bytes() const;

    bool     covers(const void *address) const;
    uint64_t host_address_of(const void *address) const;

    void     mapped();
    bool     unmapped();
    uint32_t map_depth() const;

    bool invariants_hold() const;

private:
    friend struct SvmAllocationProbe;

    uint64_t host_base_;
    uint8_t *mirror_;
    size_t   bytes_;
    uint32_t map_depth_;
};

}

#endif
