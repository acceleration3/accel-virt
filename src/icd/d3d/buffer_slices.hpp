#ifndef AV_D3D_BUFFER_SLICES_HPP
#define AV_D3D_BUFFER_SLICES_HPP

#include "core/support/status.hpp"
#include "core/support/growable_array.hpp"
#include "icd/common/range_table.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class ThreadLock;
}

namespace av::d3d {

class GpuTimeline;

class BufferSlices {
public:
    struct Slice {
        uint64_t offset_;
        uint64_t bytes_;
    };

    static constexpr uint64_t smallest = 256u;
    static constexpr uint64_t largest_small = uint64_t{64} * 1024u;
    static constexpr uint32_t small_classes = 9u;
    static constexpr uint64_t block_bytes = uint64_t{256} * 1024u;
    static constexpr uint64_t large_alignment = uint64_t{64} * 1024u;
    BufferSlices(BlockSource &memory, GpuTimeline &timeline, ThreadLock &lock, ClientTransport &wire);
    BufferSlices(const BufferSlices &) = delete;
    BufferSlices &operator=(const BufferSlices &) = delete;

    void                 take(uint64_t bytes);
    [[nodiscard]] Status take_slice(uint64_t bytes, Slice &slice);
    void                 retire(const Slice &slice);
    void                 give_back(const Slice &slice);

    uint32_t retired() const;
    uint32_t free_in_class(uint64_t bytes) const;
    uint64_t begin() const;
    uint64_t end() const;

    bool invariants_hold() const;

private:
    struct Retired {
        uint64_t offset_;
        uint64_t bytes_;
        uint64_t tag_;
    };

    static constexpr uint32_t no_class = ~0u;

    uint32_t           class_of(uint64_t bytes) const;
    uint64_t           class_bytes(uint32_t which) const;
    [[nodiscard]] Status take_once(uint64_t bytes, Slice &slice);
    [[nodiscard]] Status carve(uint32_t which);
    [[nodiscard]] Status make_retired_room();
    void               compact_retired();
    void               reclaim();
    void               free_slice(uint64_t offset, uint64_t bytes);
    bool               wait_oldest();

    GpuTimeline             &timeline_;
    ThreadLock              &lock_;
    ClientTransport         &wire_;
    icd::RangeTable          ranges_;
    GrowableArray<uint64_t>  free_[small_classes];
    uint32_t                 carved_[small_classes];
    GrowableArray<Retired>   retired_;
    uint32_t                 retired_first_;
    uint32_t                 outstanding_;
    bool                     lost_;

    friend struct BufferSlicesProbe;
};

}

#endif
