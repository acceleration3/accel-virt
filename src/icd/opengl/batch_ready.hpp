#ifndef AV_GLICD_BATCH_READY_HPP
#define AV_GLICD_BATCH_READY_HPP

#include "guest/transport/batch_mark.hpp"
#include "icd/common/frame_wait.hpp"

namespace av {
class Kmt;
}

namespace av::glicd {

class BatchReady {
public:
    using Mark = BatchMark;

    explicit BatchReady(Kmt &kmt);
    BatchReady(const BatchReady &) = delete;
    BatchReady &operator=(const BatchReady &) = delete;

    icd::FrameWait wait(const BatchMark &mark);

private:
    Kmt &kmt_;
};

}

#endif
