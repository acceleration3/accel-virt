#ifndef AV_ICD_FRAME_WAIT_HPP
#define AV_ICD_FRAME_WAIT_HPP

#include <stdint.h>

namespace av::icd {

enum class FrameWait : uint8_t { drawn, later, gone };

}

#endif
