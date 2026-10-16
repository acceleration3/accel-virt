/* How OpenGL's present queue waits for a swap's frame (WindowPresentQueue's
 * readiness): the batch that read it back, through a Kmt only the present
 * thread waits on, since a Kmt's waits share its event. */
#ifndef AV_GLICD_BATCH_READY_HPP
#define AV_GLICD_BATCH_READY_HPP

#include "guest/transport/batch_mark.hpp"
#include "icd/common/frame_wait.hpp"

namespace av {
class Kmt;
}  // namespace av

namespace av::glicd {

class BatchReady {
public:
    using Mark = BatchMark;

    explicit BatchReady(Kmt &kmt);
    BatchReady(const BatchReady &) = delete;
    BatchReady &operator=(const BatchReady &) = delete;

    /* drawn once the batch is done; later while the kernel's bounded wait
     * times out on a host that is behind; gone for a context that is. */
    icd::FrameWait wait(const BatchMark &mark);

private:
    Kmt &kmt_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BATCH_READY_HPP
