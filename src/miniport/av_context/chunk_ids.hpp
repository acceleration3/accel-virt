/* The adapter's host blob ids (HostBlobId): which are taken by a chunk a
 * process holds and which are free, so every chunk on the device has an id no
 * other resource and no other chunk has. A free id is handed out lowest
 * first; one whose blob the device would not release stays taken for good,
 * since the device may still hold a blob under it. Under the owner's lock. */
#ifndef AV_CHUNK_IDS_HPP
#define AV_CHUNK_IDS_HPP

#include "core/support/status.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <stdint.h>

namespace av {

class ChunkIds {
public:
    ChunkIds();

    /* The lowest free id, taken: no_space when every one is. */
    [[nodiscard]] Status take(uint32_t &id);
    /* `id` free again; nothing for one not taken or outside the range. */
    void                 give_back(uint32_t id);
    uint32_t             taken() const;

private:
    static constexpr uint32_t word_bits = 64u;
    static constexpr uint32_t words = HostBlobId::count / word_bits;

    uint64_t taken_[words];
};

}  // namespace av

#endif  // AV_CHUNK_IDS_HPP
