#ifndef AV_CLICD_SVM_ALLOCATION_HPP
#define AV_CLICD_SVM_ALLOCATION_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct SvmAllocationProbe;

class SvmAllocation {
public:
    SvmAllocation();

    static constexpr uint32_t most_regions = 8u;

    void take(uint64_t host_base, uint8_t *mirror, size_t bytes, uint8_t *raw = nullptr);
    void give_up();
    bool held() const;

    uint64_t host_base() const;
    uint8_t *mirror() const;
    uint8_t *raw() const;
    size_t   bytes() const;

    bool     covers(const void *address) const;
    uint64_t host_address_of(const void *address) const;

    void     mapped();
    bool     unmapped();
    void     note_region(uint64_t offset, uint64_t bytes, bool writes);
    bool     take_region(uint64_t offset, uint64_t &bytes, bool &writes);
    uint32_t map_depth() const;

    bool invariants_hold() const;

private:
    friend struct SvmAllocationProbe;

    uint64_t host_base_;
    uint8_t *mirror_;
    uint8_t *raw_;
    size_t   bytes_;
    uint32_t map_depth_;

    struct Region {
        uint64_t offset_;
        uint64_t bytes_;
        bool     writes_;
        bool     used_;
    };

    Region   regions_[most_regions];
};

}

#endif
