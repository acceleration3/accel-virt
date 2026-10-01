#include "harness/icd/framed_block_probe.hpp"

namespace av::icd {

FramedBlockProbe::FramedBlockProbe(FramedBlock &target) : framed(target) {}

size_t &FramedBlockProbe::used() { return framed.used_; }

size_t &FramedBlockProbe::capacity() { return framed.capacity_; }

uint8_t *&FramedBlockProbe::block() { return framed.block_; }

}  // namespace av::icd
