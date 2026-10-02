/* The blobs one Vulkan backend made device memory of, found again by that
 * memory.
 *
 * The session owns the blobs; the backend only needs to recognise one when the
 * guest frees its memory through the wire, so that the import is released once
 * - by the free - and not again when the device releases the blob. A blob here
 * lives in its session's ImportTable, which never moves it. */
#ifndef AV_ADOPTED_BLOBS_HPP
#define AV_ADOPTED_BLOBS_HPP

#include "core/support/status.hpp"
#include "executor/import_table.hpp"

#include <stdint.h>

namespace av {

class ImportedBlob;

class AdoptedBlobs {
public:
    /* One session's worth: a backend serves one session at a time. */
    static constexpr uint32_t capacity = ImportTable::max_imports;

    AdoptedBlobs();
    AdoptedBlobs(const AdoptedBlobs &) = delete;
    AdoptedBlobs &operator=(const AdoptedBlobs &) = delete;

    /* Remembers `blob`, which must already hold what the backend made of it.
     * invalid_parameter for one holding nothing, conflict for one already
     * here, no_space when full. */
    [[nodiscard]] Status add(ImportedBlob &blob);
    /* The blob whose memory is `object`, or null - for 0 as well. */
    ImportedBlob        *find(uint64_t object);
    /* Forgets `blob`. Nothing for one that is not here, because a blob whose
     * memory the guest already freed is abandoned again when it is released. */
    void                 remove(const ImportedBlob &blob);
    void                 clear();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct AdoptedBlobsProbe;

    ImportedBlob *blobs_[capacity];
    uint32_t      count_;
};

}  // namespace av

#endif  // AV_ADOPTED_BLOBS_HPP
