/* One context's setup or teardown on the device, as the requests it takes in
 * order (docs/protocol.md): CTX_CREATE, a guest blob each for the ring, fence
 * page and reply slot, then the setup; or CTX_DESTROY and an unref for each
 * blob. One request and one answer at a time. A refused setup stops; a
 * teardown releases exactly what the setup created - counting a request whose
 * answer was unreadable as done, since the device may have acted on it - and
 * keeps going past a refusal. At most DISPATCH. */
#ifndef AV_CONTEXT_REQUEST_SEQUENCE_HPP
#define AV_CONTEXT_REQUEST_SEQUENCE_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/context/context_setup.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextMemory;
struct ContextRequestSequenceProbe;

class ContextRequestSequence {
public:
    explicit ContextRequestSequence(Platform &platform);

    /* invalid_state unless idle - nothing set up, or all of it torn down; invalid_parameter
     * for memory not held. */
    [[nodiscard]] Status begin_setup(uint32_t context_id, const ContextSetup &setup, const ContextMemory &memory);
    /* invalid_state while a sequence runs or when nothing was created. */
    [[nodiscard]] Status begin_teardown();
    /* The next request into `bytes`. invalid_state while an answer is awaited
     * or the sequence finished; too_large, writing nothing, when it does not fit. */
    [[nodiscard]] Status next_request(uint8_t *bytes, size_t capacity, size_t &length);
    /* ok for OK_NODATA; not_supported for a refusal; malformed for an answer
     * too short to read, which ends a setup; invalid_state when none is awaited. */
    [[nodiscard]] Status accept(const uint8_t *bytes, size_t length);
    /* DISPATCH. The device reset: it holds nothing any request created. */
    void                 abandon();
    bool                 finished() const;
    /* Whether any answer was not OK_NODATA, and the first such answer's type. */
    bool                 refused() const;
    uint32_t             refusal() const;

    bool invariants_hold() const;

private:
    enum class Phase : uint8_t { idle, setting_up, set_up, tearing_down };
    static constexpr uint32_t setup_steps = 5u;
    static constexpr uint32_t blob_count = 3u;
    static constexpr uint32_t context_step = 0u;
    static constexpr uint32_t first_blob_step = 1u;
    static constexpr uint32_t setup_step = 4u;
    static constexpr uint32_t unreadable = 0u;

    [[nodiscard]] Status write_setup_request(uint8_t *bytes, size_t capacity, size_t &length);
    [[nodiscard]] Status write_teardown_request(uint8_t *bytes, size_t capacity, size_t &length);
    void                 finish_setup_step(bool done);
    void                 refuse(uint32_t type);
    uint32_t             teardown_steps() const;

    Platform    &platform_;
    Phase        phase_;
    ContextSetup setup_;
    uint32_t     context_id_;
    uint64_t     addresses_[blob_count];
    uint32_t     lengths_[blob_count];
    uint32_t     step_;
    bool         awaiting_;
    bool         context_created_;
    uint32_t     blobs_created_;
    uint32_t     refusal_;
    bool         refused_;

    friend struct ContextRequestSequenceProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_REQUEST_SEQUENCE_HPP
