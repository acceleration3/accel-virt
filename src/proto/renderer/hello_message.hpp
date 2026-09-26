/* QEMU -> renderer, the first message on a new renderer's socket: the protocol
 * QEMU speaks, which renderer this is since realize (so a message a dead one
 * was sent cannot be mistaken for this one's), the host GPU the device is
 * bound to, the executor library to load and the capset the guest was given.
 * The path and capset are views into the bytes decoded or the storage given;
 * both belong to the caller and must outlive the message. */
#ifndef AV_HELLO_MESSAGE_HPP
#define AV_HELLO_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HelloMessage {
public:
    static constexpr size_t   fixed_size = 32u;
    static constexpr size_t   uuid_bytes = 16u;
    /* PATH_MAX less its terminator: the renderer copies the path and ends it
     * before handing it to dlopen. */
    static constexpr uint32_t max_path_length = 4095u;
    static constexpr uint32_t max_capset_length = 4096u;
    static constexpr uint32_t descriptors = 0u;

    HelloMessage();
    /* A null path or capset is an empty one. */
    HelloMessage(uint32_t version, uint32_t epoch, const uint8_t (&gpu_uuid)[uuid_bytes],
                 const char *executor_path, uint32_t path_length, const uint8_t *capset,
                 uint32_t capset_length);

    uint32_t       version() const;
    uint32_t       epoch() const;
    const uint8_t *gpu_uuid() const;
    /* Not terminated: executor_path_length() bytes, none of them NUL. */
    const char    *executor_path() const;
    uint32_t       executor_path_length() const;
    const uint8_t *capset() const;
    uint32_t       capset_length() const;
    size_t         encoded_length() const;

    /* too_large for less room than encoded_length(); invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any descriptor, a path that is empty, too long or holds a
     * NUL, a capset that is empty or too long, or a length other than the
     * fields give. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t       version_;
    uint32_t       epoch_;
    uint8_t        gpu_uuid_[uuid_bytes];
    const char    *executor_path_;
    uint32_t       path_length_;
    const uint8_t *capset_;
    uint32_t       capset_length_;
};

}  // namespace av

#endif  // AV_HELLO_MESSAGE_HPP
