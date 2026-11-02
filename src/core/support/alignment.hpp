#ifndef AV_ALIGNMENT_HPP
#define AV_ALIGNMENT_HPP

#include <stdint.h>

namespace av {

class Alignment {
public:
    explicit Alignment(uint64_t value);

    bool          is_valid() const;
    uint64_t value() const;

    bool align_up(uint64_t address, uint64_t &result) const;

    bool divides(uint64_t value) const;

private:
    uint64_t value_;
};

}

#endif
