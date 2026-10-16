/* Reaches into a FramedBlock so the invariant tests of the classes that frame
 * into one can build the states it never builds. Declared a friend by
 * FramedBlock; nothing outside the tests uses it. */
#ifndef AV_FRAMED_BLOCK_PROBE_HPP
#define AV_FRAMED_BLOCK_PROBE_HPP

#include "guest/transport/framed_block.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct FramedBlockProbe {
    explicit FramedBlockProbe(FramedBlock &target);

    size_t   &used();
    size_t   &capacity();
    uint8_t *&block();

    FramedBlock &framed;
};

}  // namespace av

#endif  // AV_FRAMED_BLOCK_PROBE_HPP
