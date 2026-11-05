#ifndef AV_D3D_BATCH_COUNTER_HPP
#define AV_D3D_BATCH_COUNTER_HPP

#include <stdint.h>

namespace av {

class D3dBatchCounter {
public:
    struct Marks {
        uint64_t d3d11_;
        uint64_t d3d12_;
        uint64_t d3d9_;

        bool operator==(const Marks &other) const = default;
        bool covered_by(const Marks &done) const;
    };

    static constexpr uint32_t pending_capacity = 64u;

    D3dBatchCounter();
    D3dBatchCounter(const D3dBatchCounter &) = delete;
    D3dBatchCounter &operator=(const D3dBatchCounter &) = delete;

    void end(const Marks &issued, const Marks &done);
    void retire(const Marks &done);

    uint64_t marked() const;
    uint64_t reached(const Marks &done) const;
    bool     pending(const Marks &done) const;

    bool invariants_hold() const;

private:
    struct Record {
        uint64_t from_;
        Marks    marks_;
    };

    const Record &at(uint32_t index) const;
    Record       &newest();

    Record   pending_[pending_capacity];
    uint32_t first_;
    uint32_t count_;
    Marks    last_;
    uint64_t marked_;
};

}

#endif
