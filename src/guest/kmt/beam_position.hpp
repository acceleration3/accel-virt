#ifndef AV_BEAM_POSITION_HPP
#define AV_BEAM_POSITION_HPP

#include <stdint.h>

namespace av {

class BeamPosition {
public:
    BeamPosition();
    BeamPosition(bool in_vertical_blank, uint32_t line);

    bool earlier_than(const BeamPosition &other) const;

private:
    bool     in_vertical_blank_;
    uint32_t line_;
};

}

#endif
