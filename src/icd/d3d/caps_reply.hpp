/* One GetCaps answer written into the buffer a Direct3D runtime lent it:
 * Direct3D 9's, 11's and 12's drivers each answer every capability question
 * this way. What the runtime lent past the answer is zeroed - a newer
 * runtime's larger structure reads its later fields as "not supported" - and
 * an answer is written only where it fits.
 *
 * Two answers, because the runtimes ask two ways. A whole answer is a value
 * or structure of one size, which a buffer too small for is refused rather
 * than written past. A prefix answer is one of Direct3D 12's versioned
 * structures, each revision the last with fields appended: an older runtime
 * lends the size of the revision it knows and takes the fields it has. */
#ifndef AV_D3D_CAPS_REPLY_HPP
#define AV_D3D_CAPS_REPLY_HPP

#include <stddef.h>

namespace av::d3d {

class CapsReply {
public:
    /* The runtime's buffer, `bytes` long; null is no buffer. */
    CapsReply(void *data, size_t bytes);

    /* `answer` whole, the rest zeroed. False, and nothing written, for no
     * buffer or one shorter than the answer. */
    [[nodiscard]] bool give_whole(const void *answer, size_t answer_bytes) const;
    /* As much of `answer` as the buffer holds, the rest zeroed. False for no
     * buffer. */
    [[nodiscard]] bool give_prefix(const void *answer, size_t answer_bytes) const;
    /* All zeroes: "none" for a feature this driver does not expose. False
     * for no buffer. */
    [[nodiscard]] bool give_none() const;

private:
    void  *data_;
    size_t bytes_;
};

}  // namespace av::d3d

#endif  // AV_D3D_CAPS_REPLY_HPP
