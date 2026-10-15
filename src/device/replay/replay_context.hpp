/* One recorded context, replayed on an executor library: the executor the
 * library made for its sub-API, the commands of that API run through it in the
 * recorded order, and the answer the last reply-generating command produced,
 * kept so the recording's own reply can be compared with it.
 *
 * A context becomes unreplayable when the recording names memory the replay
 * cannot recreate; its batches are then not run, because an answer produced
 * against memory that is not there would be a wrong answer reported quietly. */
#ifndef AV_REPLAY_CONTEXT_HPP
#define AV_REPLAY_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLibrary;
class RecordedCommand;

struct ReplayContextProbe;

class ReplayContext {
public:
    /* Answers longer than this are reported rather than compared. */
    static constexpr size_t answer_bytes = 16u * 1024u;

    ReplayContext();
    ~ReplayContext();
    ReplayContext(const ReplayContext &) = delete;
    ReplayContext &operator=(const ReplayContext &) = delete;

    /* The executor `library` makes for `api` in its domain: invalid_state
     * once created, no_memory when the library makes none. */
    [[nodiscard]] Status create(const ExecutorLibrary &library, uint32_t context, SubApi api);
    void                 destroy(const ExecutorLibrary &library);
    bool                 created() const;
    uint32_t             context() const;
    /* The API half of the command ids this context sends. */
    uint16_t             command_api() const;

    /* Runs one command: ok when it ran, not_found for one the library does not
     * know, malformed for one it could not decode, invalid_state for a lost
     * device, no executor, or an answer no library may give. */
    [[nodiscard]] Status execute(const ExecutorLibrary &library, const RecordedCommand &command);
    /* The answer the last reply-generating command produced. */
    bool                 answered() const;
    size_t               answer_length() const;
    /* Whether that answer is the `length` bytes at `recorded`. */
    bool                 answers_with(const uint8_t *recorded, uint32_t length) const;
    /* The next batch starts with no answer of its own. */
    void                 forget_answer();
    /* This batch fetched part of an answer the library kept: whatever its
     * reply holds, it matches, since the replay keeps none to compare. */
    void                 answer_in_parts();

    /* Nothing of this context is replayed from here on, and why. */
    void                 give_up();
    bool                 gave_up() const;

    bool invariants_hold() const;

private:
    AvExecutor *executor_;
    uint32_t     context_;
    SubApi       api_;
    bool         answered_;
    bool         in_parts_;
    /* The length the library wanted, which may pass what was kept. */
    size_t       answer_length_;
    bool         gave_up_;
    uint8_t      answer_[answer_bytes];

    friend struct ReplayContextProbe;
};

}  // namespace av

#endif  // AV_REPLAY_CONTEXT_HPP
