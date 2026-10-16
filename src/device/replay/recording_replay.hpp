/* Replays a recording into an executor library through the same
 * av_executor.h the device drives (FR-006): a context per setup record, every
 * command of every batch in the recorded order, and each recorded reply
 * compared with the answer the replay produced.
 *
 * What it will not do is guess. A blob the recording holds no bytes for, an
 * answer longer than it can keep, a library that lost its device: each is a
 * finding naming the fence and the command, and that context runs no further,
 * because a wrong answer reported quietly is worse than no answer.
 *
 * A recording holds as many contexts as the guest made while it ran, and as
 * many findings as it differs in: both are kept in memory that grows with
 * them. */
#ifndef AV_RECORDING_REPLAY_HPP
#define AV_RECORDING_REPLAY_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/replay/replay_context.hpp"
#include "proto/context/device_uuid.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;
class ExecutorLibrary;
class RecordedCommand;
class RecordingFilter;
class RecordingReader;
class TextBuffer;

struct RecordingReplayProbe;

class RecordingReplay {
public:
    static constexpr size_t   finding_bytes = 192u;
    static constexpr size_t   uuid_bytes = DeviceUuid::bytes;

    /* Opens its own domain in `library`, on the GPU it replays on, and closes
     * it as it goes; its contexts and findings live in `memory`. */
    RecordingReplay(ExecutorLibrary &library, BlockSource &memory);
    /* Destroys every executor it made. */
    ~RecordingReplay();
    RecordingReplay(const RecordingReplay &) = delete;
    RecordingReplay &operator=(const RecordingReplay &) = delete;

    /* The GPU to ask the library for, instead of the one the recording names. */
    void use_gpu(const uint8_t *uuid);

    /* Replays what `reader` walks, showing only what `filter` keeps: ok when
     * the whole recording was replayed, malformed when it was damaged,
     * invalid_state once run. Findings are counted either way. */
    [[nodiscard]] Status run(RecordingReader &reader, const RecordingFilter &filter);

    uint32_t    contexts() const;
    uint64_t    commands() const;
    uint64_t    replies() const;
    uint32_t    findings() const;
    /* Finding `index`, or the empty string past the last kept: a finding
     * there was no memory to keep is counted but has no text. */
    const char *finding(uint32_t index) const;

    bool invariants_hold() const;

private:
    /* The context of `record`, or null when the recording named one this
     * replay never set up. */
    ReplayContext *context_of(uint32_t context);
    void           replay_setup(const RecordHeader &record, const uint8_t *payload);
    void           replay_batch(const RecordHeader &record, const uint8_t *payload,
                                const RecordingFilter &filter);
    void           replay_reply(const RecordHeader &record, const uint8_t *payload);
    void           replay_blob(const RecordHeader &record, const uint8_t *payload);
    /* A finding, opened with the context and fence it belongs to. */
    void           open_finding(TextBuffer &text, const RecordHeader &record) const;
    /* Counts the finding, and keeps its text when there is memory for it. */
    void           keep_finding(const TextBuffer &text);
    /* The commonest finding: something about a record, said in one phrase. */
    void           finding_about(const RecordHeader &record, const char *what);

    ExecutorLibrary       &library_;
    DeviceUuid             gpu_;
    bool                   gpu_chosen_;
    bool                   ran_;
    uint32_t               contexts_;
    uint64_t               commands_;
    uint64_t               replies_;
    uint32_t               findings_;
    struct Finding {
        char text_[finding_bytes];
    };

    ObjectPool<ReplayContext>             replays_;
    /* Each live replay by the context id the recording names it with. */
    KeyedSlots<uint32_t, ReplayContext *> by_context_;
    GrowableArray<Finding>                findings_text_;

    friend struct RecordingReplayProbe;
};

}  // namespace av

#endif  // AV_RECORDING_REPLAY_HPP
