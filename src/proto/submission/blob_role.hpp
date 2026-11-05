#ifndef AV_BLOB_ROLE_HPP
#define AV_BLOB_ROLE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobRole {
public:
    enum class Kind : uint32_t { plain = 0u, arena_segment = 1u };

    BlobRole();
    BlobRole(Kind kind, uint64_t base);

    Kind     kind() const;
    uint64_t base() const;
    bool     is_arena() const;
    bool     valid() const;

    [[nodiscard]] Status decode(uint32_t kind, uint64_t base);

    bool operator==(const BlobRole &other) const;

private:
    Kind     kind_;
    uint64_t base_;
};

}

#endif
