#ifndef AV_FRAME_STAGER_HPP
#define AV_FRAME_STAGER_HPP

namespace av {

class StagedFrame;

class FrameStager {
public:
    FrameStager(const FrameStager &) = delete;
    FrameStager &operator=(const FrameStager &) = delete;

    virtual StagedFrame *stage() = 0;

protected:
    FrameStager() = default;
    ~FrameStager() = default;
};

}

#endif
