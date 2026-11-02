#ifndef AV_GL_MIRROR_HPP
#define AV_GL_MIRROR_HPP

#include "executor/shim/gl_level.hpp"

#include <stdint.h>

namespace av {

class GlMirror {
public:
    GlMirror();
    GlMirror(const void *mem, uint32_t name, uint64_t bytes, bool written_back);
    GlMirror(const void *mem, uint32_t object_type, uint32_t name, uint32_t target, const GlLevel &level,
             uint64_t bytes, void *image, bool written_back);

    const void    *mem() const;
    uint32_t       object_type() const;
    bool           buffer() const;
    bool           texture() const;
    uint32_t       name() const;
    uint32_t       target() const;
    const GlLevel &level() const;
    uint64_t       bytes() const;
    void          *image() const;
    bool           written_back() const;
    bool           acquired() const;
    void           acquire();
    void           release();

    bool described() const;

private:
    const void *mem_;
    uint32_t    object_type_;
    uint32_t    name_;
    uint32_t    target_;
    GlLevel     level_;
    uint64_t    bytes_;
    void       *image_;
    bool        written_back_;
    bool        acquired_;
};

}

#endif
