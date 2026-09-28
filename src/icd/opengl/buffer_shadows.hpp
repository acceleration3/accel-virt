/* What the host holds of one context's buffers, as far as the guest can know
 * it: a copy of each range a map wrote back or read (BufferShadow), the
 * buffers whose storage was just made with no data, and each storage's size -
 * so a map asks the host neither. A map that only writes
 * starts from the copy - OpenGL keeps whatever bytes a map leaves alone - and
 * its write-back sends only the bytes that differ from it. With no copy the
 * map is filled from the host, unless the storage is fresh and holds nothing
 * worth keeping.
 *
 * A command the host runs that changes a buffer forgets that buffer's copy.
 * One that lets anything change any buffer out of sight - a binding a shader
 * writes through, another context sharing the objects - ends the copies for
 * good, and every map is filled from then on. */
#ifndef AV_GLICD_BUFFER_SHADOWS_HPP
#define AV_GLICD_BUFFER_SHADOWS_HPP

#include "core/support/object_storage.hpp"
#include "icd/opengl/buffer_shadow.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}  // namespace av

namespace av::glicd {

struct BufferShadowsProbe;

class BufferShadows {
public:
    static constexpr uint32_t max_shadows = 64u;
    /* Past this the copies least recently used go; a single range larger is
     * filled at every map instead. */
    static constexpr uint64_t max_bytes = uint64_t{64} << 20u;
    /* Fresh storage remembered; the oldest mark is lost, which costs a fill. */
    static constexpr uint32_t max_fresh = 64u;
    /* Sizes remembered; the oldest is lost, which costs the map a question. */
    static constexpr uint32_t max_sizes = 256u;

    explicit BufferShadows(BlockSource &memory);
    ~BufferShadows();
    BufferShadows(const BufferShadows &) = delete;
    BufferShadows &operator=(const BufferShadows &) = delete;

    /* The copy of `length` bytes of `buffer` from `offset`; null unless one
     * copy holds them all. */
    uint8_t *covering(uint32_t buffer, uint64_t offset, uint64_t length);
    /* The host now holds `bytes` there: into the copy that covers the range,
     * or a copy of just the range in place of the buffer's other one, room
     * made by dropping the least recently used. Nothing is kept of a range
     * past max_bytes, or once distrusted. */
    void     keep(uint32_t buffer, uint64_t offset, uint64_t length, const uint8_t *bytes);
    /* glBufferSubData: what a copy holds of the range takes the new bytes. */
    void     wrote(uint32_t buffer, uint64_t offset, uint64_t length, const void *bytes);
    /* glBufferData and glBufferStorage: the copy goes, the storage is `size`
     * bytes, and made with no data it is fresh. A negative size is refused
     * and changes nothing. */
    void     respecify(uint32_t buffer, int64_t size, bool with_data);
    /* The size the host answered for `buffer`. */
    void     sized(uint32_t buffer, uint64_t size);
    /* `buffer`'s size, when known. */
    bool     size_of(uint32_t buffer, uint64_t &size);
    /* Storage made with no data that nothing has written since: a map of it
     * has nothing to keep. */
    bool     fresh(uint32_t buffer) const;
    /* Everything known of `buffer`: a command the host runs changed it. */
    void     forget(uint32_t buffer);
    /* glDeleteBuffers. */
    void     forget(int32_t count, const uint32_t *buffers);
    void     forget_all();
    /* Nothing is kept from now on: safe from any thread, and what is held
     * goes when this context's own thread next asks. */
    void     distrust();
    bool     trusted() const;
    uint32_t held() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* The slot with `buffer`'s copy, or max_shadows. */
    uint32_t holding(uint32_t buffer) const;
    uint32_t free_slot() const;
    uint32_t least_recent() const;
    /* A free slot with room for `length` more bytes, which is at most
     * max_bytes: while there is none the least recent copy goes. */
    uint32_t make_room(uint64_t length);
    void     drop(uint32_t slot);
    /* Whether nothing is kept any more, dropping what still is. */
    bool     let_go();
    void     unmark(uint32_t buffer);
    /* The row with `buffer`'s size, or max_sizes. */
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

}  // namespace av::glicd

#endif  // AV_GLICD_BUFFER_SHADOWS_HPP
