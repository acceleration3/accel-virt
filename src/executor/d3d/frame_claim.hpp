/* Whether one staged frame is out: claimed by the session staging a present
 * into it, until the present engine reads it and gives it back. The claim is
 * one compare-and-swap, so two sessions staging on one GPU at once never
 * take the same frame - a session's thread claims, the engine's thread gives
 * back. */
#ifndef AV_FRAME_CLAIM_HPP
#define AV_FRAME_CLAIM_HPP

#include <stdint.h>

namespace av {

class FrameClaim {
public:
    FrameClaim();
    FrameClaim(const FrameClaim &) = delete;
    FrameClaim &operator=(const FrameClaim &) = delete;

    /* Free to out; false, and nothing changed, when it was out. */
    [[nodiscard]] bool claim();
    void               give_back();
    bool               out() const;

private:
    uint32_t out_;
};

}  // namespace av

#endif  // AV_FRAME_CLAIM_HPP
