#ifndef AV_GL_BATCH_PROGRESS_HPP
#define AV_GL_BATCH_PROGRESS_HPP

#include "executor/slice_progress.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class GlBatchProgress final : public SliceProgress {
public:
    static constexpr uint32_t most_pending = 32u;

    explicit GlBatchProgress(const glproto::RealGl &gl);
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

    bool invariants_hold() const;

private:
    struct Pending {
        void    *sync_;
        uint64_t mark_;
    };

    void drop_sync(void *sync);

    const glproto::RealGl &gl_;
    Pending                pending_[most_pending];
    uint32_t               first_;
    uint32_t               count_;
    uint64_t               marked_;
    uint64_t               reached_;
};

}

#endif
