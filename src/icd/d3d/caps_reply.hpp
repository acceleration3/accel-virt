#ifndef AV_D3D_CAPS_REPLY_HPP
#define AV_D3D_CAPS_REPLY_HPP

#include <stddef.h>

namespace av::d3d {

class CapsReply {
public:
    CapsReply(void *data, size_t bytes);

    [[nodiscard]] bool give_whole(const void *answer, size_t answer_bytes) const;
    [[nodiscard]] bool give_prefix(const void *answer, size_t answer_bytes) const;
    [[nodiscard]] bool give_none() const;

private:
    void  *data_;
    size_t bytes_;
};

}

#endif
