/* One guest context, running on one host API.
 *
 * What the device holds as an opaque `RvkExecutor *`. It owns the block one
 * command decodes into and, later, the blobs that context imported; it does
 * not own the backend, which outlives a single session no more than it
 * outlives the GPU. */
#ifndef AV_CONTEXT_SESSION_HPP
#define AV_CONTEXT_SESSION_HPP

#include "core/support/status.hpp"
#include "device/executor/decode_arena.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SessionBackend;

class ContextSession {
public:
    ContextSession();
    ContextSession(const ContextSession &) = delete;
    ContextSession &operator=(const ContextSession &) = delete;

    /* Binds this session to a backend and to the block its commands decode
     * into. invalid_state if it is already bound. */
    [[nodiscard]] Status initialize(SessionBackend &backend, uint8_t *block, size_t block_bytes);
    /* Releases the session for reuse. Safe on one that never ran. */
    void                 release();
    bool                 live() const;

    /* Runs one command and answers one of the ABI's four results. `reply` may
     * be null with a capacity of zero, which is what the device passes for
     * every command that did not ask for an answer; `answer` is then the
     * length the answer would have been, which is what lets the device tell
     * the guest what to allocate and ask again. */
    uint32_t execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                     size_t reply_capacity, size_t &answer);

    /* Commands this session refused because the block was too small rather
     * than because the frame was bad. The guest is told the same thing either
     * way; this is what an operator sees. */
    size_t   starved() const;

    bool invariants_hold() const;

private:
    SessionBackend *backend_;
    DecodeArena     arena_;
    size_t          starved_;
    bool            faulted_;
};

}  // namespace av

#endif  // AV_CONTEXT_SESSION_HPP
