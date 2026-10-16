/* The adapter's host blob ids (HostBlobId): which are taken by a chunk a
 * process holds and which are free, so every chunk on the device has an id no
 * other resource and no other chunk has. A free id is handed out lowest
 * first; one whose blob the device would not release stays taken for good,
 * since the device may still hold a blob under it.
 *
 * The bitmap grows a word at a time as ids are taken, so there is no count of
 * chunks past which an open is refused: what bounds them is the host window
 * each takes pages of. PASSIVE, under the owner's lock: taking grows the
 * bitmap; giving back never allocates. */
#ifndef AV_CHUNK_IDS_HPP
#define AV_CHUNK_IDS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Platform;
struct ChunkIdsProbe;

class ChunkIds {
public:
    /* `most` ids from HostBlobId::first can be handed out: all of HostBlobId's
     * range on the adapter. */
    ChunkIds(Platform &platform, uint32_t most);
    ChunkIds(const ChunkIds &) = delete;
    ChunkIds &operator=(const ChunkIds &) = delete;

    /* The lowest free id, taken: no_space when all `most` are, no_memory when
     * the bitmap cannot grow to note one more. `id` untouched unless ok. */
    [[nodiscard]] Status take(uint32_t &id);
    /* `id` free again; nothing for one not taken or outside the range. */
    void                 give_back(uint32_t id);
    uint32_t             taken() const;

    /* The count is the bits set, no bit past `most` is set, and no word below
     * the lowest one searched has a free bit. */
    bool invariants_hold() const;

private:
    static constexpr uint32_t word_bits = 64u;

    /* Bit `index`'s word set, and the count and the search start kept. */
    void mark(uint32_t index);

    Platform               &platform_;
    GrowableArray<uint64_t> words_;
    uint32_t                most_;
    uint32_t                taken_;
    /* No word below this has a free bit. */
    uint32_t                lowest_;

    friend struct ChunkIdsProbe;
};

}  // namespace av

#endif  // AV_CHUNK_IDS_HPP
