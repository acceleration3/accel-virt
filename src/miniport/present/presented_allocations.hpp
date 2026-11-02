#ifndef AV_PRESENTED_ALLOCATIONS_HPP
#define AV_PRESENTED_ALLOCATIONS_HPP

#include <stdint.h>

namespace av {

struct DxgkargPresentRecord;

class PresentedAllocations {
public:
    explicit PresentedAllocations(const DxgkargPresentRecord &request);
    PresentedAllocations(const PresentedAllocations &) = delete;
    PresentedAllocations &operator=(const PresentedAllocations &) = delete;

    void *source() const;
    void *destination() const;

private:
    enum : uint32_t { source_index = 1u, destination_index = 2u };

    bool  fixed() const;
    void *at(uint32_t index) const;

    const DxgkargPresentRecord &request_;
};

}

#endif
