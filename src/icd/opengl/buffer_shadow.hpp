/* One range of a buffer object as the host holds it (BufferShadows): the
 * bytes the guest last wrote there or read from there, and when it last
 * looked, so the copy used least recently is the first to go. */
#ifndef AV_GLICD_BUFFER_SHADOW_HPP
#define AV_GLICD_BUFFER_SHADOW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}  // namespace av

namespace av::glicd {

struct BufferShadowProbe;

class BufferShadow {
public:
    explicit BufferShadow(BlockSource &memory);
    ~BufferShadow();
    BufferShadow(const BufferShadow &) = delete;
    BufferShadow &operator=(const BufferShadow &) = delete;

    /* A copy of `bytes` as `length` bytes of `buffer` from `offset`, used at
     * `stamp`. invalid_state when already holding; invalid_parameter for no
     * buffer or no bytes; no_memory. */
    [[nodiscard]] Status hold(uint32_t buffer, uint64_t offset, uint64_t length, const uint8_t *bytes,
                              uint64_t stamp);
    uint32_t buffer() const;
    uint64_t length() const;
    uint64_t stamp() const;
    /* Whether it holds every byte of `length` from `offset` of `buffer`. */
    bool     covers(uint32_t buffer, uint64_t offset, uint64_t length) const;
    /* Where the buffer's byte at `offset` is copied, which it must cover;
     * the copy counts as used at `stamp`. */
    uint8_t *at(uint64_t offset, uint64_t stamp);
    /* What of `length` bytes from `offset` of the buffer it holds becomes
     * those of `bytes`. Both come from a GL size, so their sum fits. */
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

}  // namespace av::glicd

#endif  // AV_GLICD_BUFFER_SHADOW_HPP
