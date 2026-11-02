#ifndef AV_GLICD_BUFFER_SHADOW_HPP
#define AV_GLICD_BUFFER_SHADOW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::glicd {

struct BufferShadowProbe;

class BufferShadow {
public:
    explicit BufferShadow(BlockSource &memory);
    ~BufferShadow();
    BufferShadow(const BufferShadow &) = delete;
    BufferShadow &operator=(const BufferShadow &) = delete;

    [[nodiscard]] Status hold(uint32_t buffer, uint64_t offset, uint64_t length, const uint8_t *bytes,
                              uint64_t stamp);
    uint32_t buffer() const;
    uint64_t length() const;
    uint64_t stamp() const;
    bool     covers(uint32_t buffer, uint64_t offset, uint64_t length) const;
    uint8_t *at(uint64_t offset, uint64_t stamp);
    void     overwrite(uint64_t offset, uint64_t length, const uint8_t *bytes);

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    BlockSource &memory_;
    uint8_t     *block_;
    uint32_t     buffer_;
    uint64_t     offset_;
    uint64_t     length_;
    uint64_t     stamp_;

    friend struct BufferShadowProbe;
};

}

#endif
