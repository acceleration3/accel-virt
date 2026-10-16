/* GL_AMD_pinned_memory: buffers whose storage is the application's own
 * memory. The application binds a buffer to GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD
 * and gives it data with glBufferData; that data is the storage from then on.
 * The pages under it are lent to the device, which makes them host memory
 * where they are, and the host's buffer is given storage over the memory
 * object its import is (docs/host-memory.md, "Application memory"). The
 * binding point is the ICD's alone: the host never sees it.
 *
 * A pinned buffer deleted while the host may still use it keeps its pages
 * lent until the context has finished its work (HostChannel::retire_memory,
 * reclaimed after glFinish). */
#ifndef AV_GLICD_PINNED_BUFFERS_HPP
#define AV_GLICD_PINNED_BUFFERS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class HostAllocation;
}  // namespace av

namespace av::icd {
class HostChannel;
}  // namespace av::icd

namespace av::glicd {

class PinnedBuffers {
public:
    /* GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD. */
    static constexpr uint32_t target = 0x9160u;

    PinnedBuffers(BlockSource &memory, icd::HostChannel &channel);
    PinnedBuffers(const PinnedBuffers &) = delete;
    PinnedBuffers &operator=(const PinnedBuffers &) = delete;

    void     bind(uint32_t buffer);
    /* The buffer bound to the target; 0 for none. */
    uint32_t bound() const;
    /* The pages under the application's `bytes` at `address` lent and kept
     * for the bound buffer; where its bytes start in what was lent in
     * `offset`. invalid_state with no buffer bound, conflict for one already
     * pinned, no_memory when it cannot be kept, and the lending's refusal. */
    [[nodiscard]] Status pin(const void *address, uint64_t bytes, HostAllocation *&lent, uint64_t &offset);
    /* The pinned among `buffers`, deleted: their pages given back once the
     * host's work over them is done, and the binding gone with them. */
    void                 forget(int32_t count, const uint32_t *buffers);
    /* What was let go of, given back: the context finished its work. */
    void                 reclaim();

private:
    static constexpr uint64_t page_bytes = 4096u;

    icd::HostChannel                       &channel_;
    uint32_t                                bound_;
    KeyedSlots<uint32_t, HostAllocation *>  pinned_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_PINNED_BUFFERS_HPP
