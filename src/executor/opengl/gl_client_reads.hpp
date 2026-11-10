#ifndef AV_GL_CLIENT_READS_HPP
#define AV_GL_CLIENT_READS_HPP

#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <stdint.h>

namespace av {

class GlClientReads {
public:
    GlClientReads();
    GlClientReads(const GlClientReads &) = delete;
    GlClientReads &operator=(const GlClientReads &) = delete;

    bool wants(uint32_t type) const;
    bool run(const glproto::RealGl &gl, WireReader payload, WireWriter &reply, WireArena &arena);
    void release(const glproto::RealGl &gl);

    bool invariants_hold() const;

private:
    uint32_t scratch_;
};

}

#endif
