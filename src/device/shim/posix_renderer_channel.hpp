/* The renderer's end of its socket to QEMU: one SOCK_SEQPACKET datagram per
 * message, the descriptors it carries passed beside it (SCM_RIGHTS). */
#ifndef AV_POSIX_RENDERER_CHANNEL_HPP
#define AV_POSIX_RENDERER_CHANNEL_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PosixRendererChannel {
public:
    /* As many as any message carries - an interrupt line per doorbell page -
     * so a truncated set is a refused message. */
    static constexpr uint32_t max_descriptors = 64u;

    explicit PosixRendererChannel(int socket);
    ~PosixRendererChannel();
    PosixRendererChannel(const PosixRendererChannel &) = delete;
    PosixRendererChannel &operator=(const PosixRendererChannel &) = delete;

    int descriptor() const;
    /* One datagram and its descriptors, now the caller's. not_found once QEMU
     * has gone; too_large for a datagram longer than `capacity` or with more
     * descriptors than fit, whose descriptors are closed. */
    [[nodiscard]] Status receive(uint8_t *buffer, size_t capacity, size_t &length, int *descriptors,
                                 uint32_t &count);
    /* One datagram and `count` descriptors beside it, which stay the
     * caller's: not_found once QEMU has gone. */
    [[nodiscard]] Status send(const uint8_t *bytes, size_t length, const int *descriptors, uint32_t count);

private:
    int socket_;
};

}  // namespace av

#endif  // AV_POSIX_RENDERER_CHANNEL_HPP
