#ifndef AV_GL_BATCH_PROGRESS_HPP
#define AV_GL_BATCH_PROGRESS_HPP

#include "executor/slice_progress.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class EventfdWake;
class GlNativeFences;

class GlBatchProgress final : public SliceProgress {
public:
    static constexpr uint32_t most_pending = 32u;

    GlBatchProgress(const glproto::RealGl &gl, const GlNativeFences &natives, EventfdWake &wake);
    GlBatchProgress(const GlBatchProgress &) = delete;
    GlBatchProgress &operator=(const GlBatchProgress &) = delete;

    void fence();
    bool settle();
    void release();

    bool fences() const;

    uint64_t next() const override;
    uint64_t reached() const override;
    uint64_t marked() const;
    bool     pending() const;
    bool     polled() const;

    bool invariants_hold() const;

private:
    struct Pending {
        void    *sync_;
        int      fd_;
        bool     watched_;
        uint64_t mark_;
    };

    bool glsync_fences() const;
    bool signalled(const Pending &row) const;
    void drop(Pending &row);

    const glproto::RealGl &gl_;
    const GlNativeFences  &natives_;
    EventfdWake           &wake_;
    Pending                pending_[most_pending];
    uint32_t               first_;
    uint32_t               count_;
    uint64_t               marked_;
    uint64_t               reached_;
};

}

#endif
