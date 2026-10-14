/* Guest blobs of any size over guest RAM the maker adds to a MockHost, as a
 * guest driver makes a context's ring, fence page and reply slot: each one a
 * single run of fresh pages, laid out one after another. For tests that need
 * a blob the device reads and writes, without spelling out its memory entries.
 * Main loop. */
#ifndef AV_GUEST_BLOB_MAKER_HPP
#define AV_GUEST_BLOB_MAKER_HPP

#include "core/support/status.hpp"

#include <cstdint>

namespace av {

class BlobTable;
class MockHost;

class GuestBlobMaker {
public:
    /* Where the maker's guest RAM lies, apart from what a test adds itself. */
    static constexpr std::uint64_t base = 0x70000000u;
    static constexpr std::uint64_t bytes = 16u << 20u;

    GuestBlobMaker(MockHost &host, BlobTable &blobs);

    /* Blob `id` of `size` bytes over the next pages: the table's answer. */
    [[nodiscard]] Status create(std::uint32_t id, std::uint64_t size);
    /* The guest RAM the last blob made covers. */
    std::uint8_t        *last_pages() const;

private:
    static constexpr std::uint64_t page = 4096u;

    MockHost     &host_;
    BlobTable    &blobs_;
    std::uint64_t next_;
    std::uint64_t last_;
};

}  // namespace av

#endif  // AV_GUEST_BLOB_MAKER_HPP
