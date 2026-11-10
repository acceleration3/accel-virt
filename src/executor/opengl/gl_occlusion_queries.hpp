#ifndef AV_GL_OCCLUSION_QUERIES_HPP
#define AV_GL_OCCLUSION_QUERIES_HPP

#include "proto/wire/wire_reader.hpp"

#include <stdint.h>

namespace av {

class GlOcclusionQueries {
public:
    GlOcclusionQueries();

    void ran(uint32_t command_id, WireReader payload);
    bool open() const;
    void clear();

    bool invariants_hold() const;

private:
    static constexpr uint32_t targets = 3u;

    bool     occlusion(uint64_t target, uint32_t &slot) const;
    uint32_t active_[targets];
};

}

#endif
