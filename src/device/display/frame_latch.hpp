/* Which frame one head shows: the newest the guest posted, taken at a vertical
 * blank and not before. A post between two blanks replaces the one before it;
 * a blank with nothing new posted keeps the frame it had. What was latched is
 * shown once, on the main loop, after the blank that took it. */
#ifndef AV_FRAME_LATCH_HPP
#define AV_FRAME_LATCH_HPP

#include <stdint.h>

namespace av {

class FrameLatch {
public:
    FrameLatch();

    /* The guest's frame `frame`, the host image published under `key`. Frame
     * 0 is no frame, and posts nothing. */
    void     post(uint32_t frame, uint64_t key);
    /* A blank: the posted frame, if there is one newer than the latched one,
     * becomes the latched one. True when it did. */
    bool     latch();
    /* The frame the last blank put on screen; 0 before the first. */
    uint32_t latched() const;
    /* The latched frame's key, once: true, with the key, when it has not been
     * shown since it was latched. */
    bool     take_unshown(uint64_t &key);
    /* A virtio reset: nothing posted, latched or waiting to be shown. */
    void     clear();

    bool invariants_hold() const;

private:
    uint32_t pending_frame_;
    uint64_t pending_key_;
    uint32_t latched_frame_;
    uint64_t latched_key_;
    bool     unshown_;
};

}  // namespace av

#endif  // AV_FRAME_LATCH_HPP
