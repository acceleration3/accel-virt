#ifndef AV_GLICD_BUFFER_MAPPING_HPP
#define AV_GLICD_BUFFER_MAPPING_HPP

#include "core/support/status.hpp"
#include "icd/common/staged_mapping.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}

namespace av::glicd {

struct BufferMappingProbe;

class BufferMapping : public icd::StagedMapping {
public:
    enum : uint32_t {
        map_read = 0x0001u, map_write = 0x0002u, map_invalidate_range = 0x0004u, map_invalidate_buffer = 0x0008u,
        map_flush_explicit = 0x0010u, map_unsynchronized = 0x0020u, map_persistent = 0x0040u,
    };
    static constexpr uint64_t max_length = uint64_t{1} << 30u;

    BufferMapping();
    BufferMapping(uint32_t buffer, uint64_t offset, uint64_t length, uint32_t access);

    uint32_t buffer() const;
    uint32_t access() const;
    bool     persistent_writes() const;
    [[nodiscard]] Status mark(uint64_t offset, uint64_t length);
    void     take_marked(uint64_t &from, uint64_t &to);

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    uint64_t dirty_from_;
    uint64_t dirty_to_;

    friend struct BufferMappingProbe;
};

}

#endif
