#ifndef AV_UM_FLIP_SHOWN_WAIT_HPP
#define AV_UM_FLIP_SHOWN_WAIT_HPP

#include "core/support/status.hpp"
#include "guest/kmt/beam_position.hpp"
#include "icd/common/once_latch.hpp"

#include <stdint.h>

namespace av {

class KeyedLog;

}

namespace av::d3d {

class D3dSession;

class FlipShownWait {
public:
    FlipShownWait(D3dSession &session, KeyedLog &lines);
    FlipShownWait(const FlipShownWait &) = delete;
    FlipShownWait &operator=(const FlipShownWait &) = delete;

    void flipped(uint32_t source);
    void await();

private:
    [[nodiscard]] Status await_blank();
    uint64_t             milliseconds() const;

    D3dSession    &session_;
    KeyedLog      &lines_;
    uint32_t       source_;
    bool           pending_;
    BeamPosition   flipped_;
    uint64_t       flipped_at_;
    uint64_t       frequency_;
    icd::OnceLatch refused_;
};

}

#endif
