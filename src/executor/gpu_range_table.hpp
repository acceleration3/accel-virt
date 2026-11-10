#ifndef AV_GPU_RANGE_TABLE_HPP
#define AV_GPU_RANGE_TABLE_HPP

#include <stdint.h>

namespace av {

class GpuRangeTable {
public:
    struct Row {
        uint64_t guest_;
        uint64_t bytes_;
        uint64_t host_;
    };

    static constexpr uint64_t header_bytes = 16u;

    explicit GpuRangeTable(uint64_t fallback);

    uint64_t bytes_for(uint32_t rows) const;
    uint64_t fallback() const;
    [[nodiscard]] bool fill(void *out, uint64_t capacity, const Row *rows, uint32_t count) const;
    bool lookup(const void *table, uint64_t guest, uint64_t &host, uint64_t &room) const;

private:
    uint64_t fallback_;
};

}

#endif
