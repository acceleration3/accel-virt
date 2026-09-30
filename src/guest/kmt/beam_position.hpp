/* Where a source's scan-out was when it was read (D3DKMTGetScanLine): a line,
 * and whether that line lay in the vertical blank. A refresh begins with its
 * blank - the driver's vsync, what DWM composes on - so within one refresh
 * every blank line comes before every visible one, and lines rise within each. */
#ifndef AV_BEAM_POSITION_HPP
#define AV_BEAM_POSITION_HPP

#include <stdint.h>

namespace av {

class BeamPosition {
public:
    BeamPosition();
    BeamPosition(bool in_vertical_blank, uint32_t line);

    /* Whether this lies earlier in its refresh than `other` in its own: read
     * after `other`, and less than a refresh after it, a blank came between. */
    bool earlier_than(const BeamPosition &other) const;

private:
    bool     in_vertical_blank_;
    uint32_t line_;
};

}  // namespace av

#endif  // AV_BEAM_POSITION_HPP
