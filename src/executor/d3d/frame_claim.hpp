#ifndef AV_FRAME_CLAIM_HPP
#define AV_FRAME_CLAIM_HPP

#include <stdint.h>

namespace av {

class FrameClaim {
public:
    FrameClaim();
    FrameClaim(const FrameClaim &) = delete;
    FrameClaim &operator=(const FrameClaim &) = delete;

    [[nodiscard]] bool claim();
    void               give_back();
    bool               out() const;

private:
    uint32_t out_;
};

}

#endif
