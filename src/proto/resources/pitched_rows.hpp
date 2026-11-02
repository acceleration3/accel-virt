#ifndef AV_PITCHED_ROWS_HPP
#define AV_PITCHED_ROWS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class PitchedRows {
public:
    PitchedRows(uint8_t *base, uint64_t pitch, uint64_t bytes);
    PitchedRows(const uint8_t *base, uint64_t pitch, uint64_t bytes);

    uint32_t copy_from(const PitchedRows &from, uint32_t rows);
    uint32_t copy_lines(const PitchedRows &from, uint32_t rows, uint64_t line);

private:
    bool holds(uint32_t row, uint64_t line) const;

    uint8_t       *base_;
    const uint8_t *source_;
    uint64_t       pitch_;
    uint64_t       bytes_;
};

}

#endif
