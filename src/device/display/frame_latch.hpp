#ifndef AV_FRAME_LATCH_HPP
#define AV_FRAME_LATCH_HPP

#include <stdint.h>

namespace av {

class FrameLatch {
public:
    FrameLatch();

    void     post(uint32_t frame, uint64_t key);
    bool     latch();
    uint32_t latched() const;
    bool     take_unshown(uint64_t &key);
    void     clear();

    bool invariants_hold() const;

private:
    uint32_t pending_frame_;
    uint64_t pending_key_;
    uint32_t latched_frame_;
    uint64_t latched_key_;
    bool     unshown_;
};

}

#endif
