#ifndef AV_GUEST_BLOB_MAKER_HPP
#define AV_GUEST_BLOB_MAKER_HPP

#include "core/support/status.hpp"

#include <cstdint>

namespace av {

class BlobTable;
class MockHost;

class GuestBlobMaker {
public:
    static constexpr std::uint64_t base = 0x70000000u;
    static constexpr std::uint64_t bytes = 16u << 20u;

    GuestBlobMaker(MockHost &host, BlobTable &blobs);

    [[nodiscard]] Status create(std::uint32_t id, std::uint64_t size);
    std::uint8_t        *last_pages() const;

private:
    static constexpr std::uint64_t page = 4096u;

    MockHost     &host_;
    BlobTable    &blobs_;
    std::uint64_t next_;
    std::uint64_t last_;
};

}

#endif
