#ifndef AV_POSIX_RENDERER_CHANNEL_HPP
#define AV_POSIX_RENDERER_CHANNEL_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PosixRendererChannel {
public:
    static constexpr uint32_t max_descriptors = 64u;

    explicit PosixRendererChannel(int socket);
    ~PosixRendererChannel();
    PosixRendererChannel(const PosixRendererChannel &) = delete;
    PosixRendererChannel &operator=(const PosixRendererChannel &) = delete;

    int descriptor() const;
    [[nodiscard]] Status receive(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                                 uint32_t &count);
    [[nodiscard]] Status send(const uint8_t *bytes, size_t length, const int *descriptors, uint32_t count);

private:
    int socket_;
};

}

#endif
