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

}

#endif
