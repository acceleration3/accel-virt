/* A GL texture's internal format as OpenCL names the same texels, and as a
 * pixel transfer moves them: the channel order and type cl_khr_gl_sharing's
 * format table gives it, and the format, type and size glGetTexImage and
 * glTexSubImage take it in (CopiedGlSharing). Only the formats that table maps
 * - one, two or four channels, normalized, integer or float - and an unsized
 * RGBA, RG or RED as the eight-bit format GL makes of it. */
#ifndef AV_GL_CL_FORMAT_HPP
#define AV_GL_CL_FORMAT_HPP

#include <stdint.h>

namespace av {

class GlClFormat {
public:
    explicit GlClFormat(uint32_t internal_format);

    /* Whether OpenCL has an image format for it at all. */
    bool     known() const;
    /* cl_channel_order and cl_channel_type; 0 for an unknown format. */
    uint32_t order() const;
    uint32_t type() const;
    /* The GL format and type a transfer of its texels names, packed as OpenCL
     * reads and writes an image of order() and type(); 0 for an unknown
     * format. */
    uint32_t transfer_format() const;
    uint32_t transfer_type() const;
    /* Bytes one texel takes in that transfer; 0 for an unknown format. */
    uint32_t texel_bytes() const;

private:
    uint32_t order_;
    uint32_t type_;
    uint32_t transfer_format_;
    uint32_t transfer_type_;
    uint32_t texel_bytes_;
};

}  // namespace av

#endif  // AV_GL_CL_FORMAT_HPP
