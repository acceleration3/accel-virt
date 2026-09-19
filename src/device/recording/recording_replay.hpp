/* Replays a recording into an executor library through the same
 * rvk_executor.h the device drives (FR-006): a context per setup record, every
 * command of every batch in the recorded order, and each recorded reply
 * compared with the answer the replay produced.
 *
 * What it will not do is guess. A blob the recording holds no bytes for, an
 * answer longer than it can keep, a library that lost its device: each is a
 * finding naming the fence and the command, and that context runs no further,
 * because a wrong answer reported quietly is worse than no answer. */
#ifndef AV_RECORDING_REPLAY_HPP
#define AV_RECORDING_REPLAY_HPP

#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/replay_context.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLibrary;
class RecordedCommand;
class RecordingFilter;
class RecordingReader;
class TextBuffer;

struct RecordingReplayProbe;

class RecordingReplay {
public:
    static constexpr uint32_t max_contexts = 8u;
    static constexpr uint32_t max_findings = 32u;
    static constexpr size_t   finding_bytes = 192u;
    static constexpr size_t   uuid_bytes = 16u;

    explicit RecordingReplay(const ExecutorLibrary &library);
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
    /* Finding `index`, or the empty string past the last. */
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
    /* Counts the finding, and keeps its text while there is room for it. */
    void           keep_finding(const TextBuffer &text);
    /* The commonest finding: something about a record, said in one phrase. */
    void           finding_about(const RecordHeader &record, const char *what);

    const ExecutorLibrary &library_;
    uint8_t                gpu_[uuid_bytes];
    bool                   gpu_chosen_;
    bool                   ran_;
    uint32_t               contexts_;
    uint64_t               commands_;
    uint64_t               replies_;
    uint32_t               findings_;
    ReplayContext          slots_[max_contexts];
    char                   findings_text_[max_findings][finding_bytes];

    friend struct RecordingReplayProbe;
};

}  // namespace av

#endif  // AV_RECORDING_REPLAY_HPP
