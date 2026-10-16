/* What a shared-object transport command answers (docs/protocol.md, Transport
 * commands; feature 053, data-model.md).
 *
 * A refusal here is an answer and not a fault: the guest asked for something
 * the device would not do, the context keeps running, and the guest's ICD
 * turns the status into its own API's error. Only a body of the wrong length
 * faults, which is framing and not a request at all. */
#ifndef AV_SHARED_OBJECT_ANSWER_HPP
#define AV_SHARED_OBJECT_ANSWER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedObjectAnswer {
public:
    static constexpr size_t encoded_size = 16u;

    enum : uint32_t {
        done = 0u,
        /* A field no request may have: no id, an unknown kind, a reserved
         * word, or a share that names no wire handle. */
        refused_request = 1u,
        /* SHARE: a wire handle this context's executor never made. OPEN and
         * RELEASE: an object this device does not hold. */
        unknown_object = 2u,
        /* The kind or the size disagrees with the object the device holds. */
        mismatched_object = 3u,
        /* The executor refused the export or the import, or its library is
         * older than the ABI version that has them. */
        executor_refused = 4u,
        /* The device or the context holds as many as it can, or the host has
         * no memory for one more. */
        no_room = 5u,
        /* SHARE: this id is already published with a different descriptor.
         * OPEN: this context already holds it, made into the same object. */
        conflict = 6u,
    };

    SharedObjectAnswer();
    SharedObjectAnswer(uint32_t status, uint64_t wire_handle);

    uint32_t status() const;
    uint64_t wire_handle() const;
    /* Whether it is the answer of a command that did what was asked. */
    bool     succeeded() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a short buffer, a non-zero reserved field, a status past
     * conflict, or a refusal carrying a wire handle - a guest that believed
     * one would name an object the device never made. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    uint32_t status_;
    uint64_t wire_handle_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_ANSWER_HPP
