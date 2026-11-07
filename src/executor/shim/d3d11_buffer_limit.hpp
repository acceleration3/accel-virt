#ifndef AV_D3D11_BUFFER_LIMIT_HPP
#define AV_D3D11_BUFFER_LIMIT_HPP

#include <stdint.h>

namespace av {

class D3d11BufferLimit {
public:
    static constexpr uint32_t least_mebibytes = 128u;
    static constexpr uint32_t most_mebibytes = 2048u;

    explicit D3d11BufferLimit(uint64_t video_memory_bytes);

    uint32_t mebibytes() const;

private:
    uint64_t video_memory_;
};

}

#endif
