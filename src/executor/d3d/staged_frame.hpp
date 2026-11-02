#ifndef AV_STAGED_FRAME_HPP
#define AV_STAGED_FRAME_HPP

#include "device/abi/av_executor.h"
#include "executor/d3d/frame_claim.hpp"

#include <stdint.h>

namespace av {

class StagedFrame {
public:
    enum class Reading { mapped, copying, failed };
    enum class Copying { copied, declined, timed_out };

    StagedFrame(const StagedFrame &) = delete;
    StagedFrame &operator=(const StagedFrame &) = delete;

    [[nodiscard]] bool claim();
    bool               out() const;
    Reading            read(AvFrameView &view);
    Copying            copy(const AvFrameCopy &target, uint32_t wait_ms);
    void               release();
    void               forsake();

protected:
    StagedFrame();
    ~StagedFrame() = default;

    void     size(uint32_t width, uint32_t height);
    uint32_t width() const;
    uint32_t height() const;
    bool     forsaken() const;
    void     readmit();

    virtual Reading map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch) = 0;
    virtual void    unmap() = 0;
    virtual Copying copy_into(const AvFrameCopy &target, uint32_t wait_ms);

private:
    FrameClaim claim_;
    uint32_t   width_;
    uint32_t   height_;
    bool       forsaken_;
};

}

#endif
