#ifndef AV_GL_FENCE_DEFERRAL_HPP
#define AV_GL_FENCE_DEFERRAL_HPP

#include "executor/opengl/gl_occlusion_queries.hpp"
#include "proto/wire/wire_reader.hpp"

#include <stdint.h>
#include <vector>

namespace av {

class GlFenceDeferral {
public:
    static constexpr uint32_t most_owed = 64u;

    GlFenceDeferral();

    void ran(uint32_t command_id, WireReader payload);
    bool defers() const;
    bool demands(uint32_t command_id) const;
    bool occlusion_open() const;
    bool advanced_blending() const;
    bool advanced_source() const;

    void     owe();
    void     paid();
    uint32_t owed() const;
    void     clear();

    bool invariants_hold() const;

private:
    static constexpr uint32_t buffers = 8u;

    void blend_enable(uint64_t cap, bool on, uint64_t index, bool indexed);
    void blend_equation(uint64_t buffer, uint64_t mode, bool indexed);

    GlOcclusionQueries occlusion_;
    bool               blending_[buffers];
    bool               advanced_[buffers];
    bool               advanced_source_;
    uint32_t           owed_;
};

}

#endif
