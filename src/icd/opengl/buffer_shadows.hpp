#ifndef AV_GLICD_BUFFER_SHADOWS_HPP
#define AV_GLICD_BUFFER_SHADOWS_HPP

#include "core/support/object_storage.hpp"
#include "icd/opengl/buffer_shadow.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::glicd {

struct BufferShadowsProbe;

class BufferShadows {
public:
    static constexpr uint32_t max_shadows = 64u;
    static constexpr uint64_t max_bytes = uint64_t{64} << 20u;
    static constexpr uint32_t max_fresh = 64u;
    static constexpr uint32_t max_sizes = 256u;

    explicit BufferShadows(BlockSource &memory);
    ~BufferShadows();
    BufferShadows(const BufferShadows &) = delete;
    BufferShadows &operator=(const BufferShadows &) = delete;

    uint8_t *covering(uint32_t buffer, uint64_t offset, uint64_t length);
    void     keep(uint32_t buffer, uint64_t offset, uint64_t length, const uint8_t *bytes);
    void     wrote(uint32_t buffer, uint64_t offset, uint64_t length, const void *bytes);
    void     respecify(uint32_t buffer, int64_t size, bool with_data);
    void     sized(uint32_t buffer, uint64_t size);
    bool     size_of(uint32_t buffer, uint64_t &size);
    bool     fresh(uint32_t buffer) const;
    void     forget(uint32_t buffer);
    void     forget(int32_t count, const uint32_t *buffers);
    void     forget_all();
    void     distrust();
    bool     trusted() const;
    uint32_t held() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    uint32_t holding(uint32_t buffer) const;
    uint32_t free_slot() const;
    uint32_t least_recent() const;
    uint32_t make_room(uint64_t length);
    void     drop(uint32_t slot);
    bool     let_go();
    void     unmark(uint32_t buffer);
    uint32_t size_row(uint32_t buffer) const;

    BlockSource                &memory_;
    ObjectStorage<BufferShadow> shadows_[max_shadows];
    uint32_t                    fresh_[max_fresh];
    uint32_t                    next_fresh_;
    uint32_t                    size_names_[max_sizes];
    uint64_t                    sizes_[max_sizes];
    uint32_t                    next_size_;
    uint64_t                    held_bytes_;
    uint64_t                    clock_;
    bool                        distrusted_;

    friend struct BufferShadowsProbe;
};

}

#endif
