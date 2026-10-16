/* What a wait for a handed frame's drawing found (WindowPresentQueue's
 * readiness policy: OpenGL's BatchReady, Vulkan's FenceReady): drawn; not yet,
 * the wait bounded and to be asked again; or never, its context or device
 * lost. */
#ifndef AV_ICD_FRAME_WAIT_HPP
#define AV_ICD_FRAME_WAIT_HPP

#include <stdint.h>

namespace av::icd {

enum class FrameWait : uint8_t { drawn, later, gone };

}  // namespace av::icd

#endif  // AV_ICD_FRAME_WAIT_HPP
