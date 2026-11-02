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

    RecordingReplay(ExecutorLibrary &library, BlockSource &memory);
    ~RecordingReplay();
    RecordingReplay(const RecordingReplay &) = delete;
    RecordingReplay &operator=(const RecordingReplay &) = delete;

    void use_gpu(const uint8_t *uuid);

    [[nodiscard]] Status run(RecordingReader &reader, const RecordingFilter &filter);

    uint32_t    contexts() const;
    uint64_t    commands() const;
    uint64_t    replies() const;
    uint32_t    findings() const;
    const char *finding(uint32_t index) const;

    bool invariants_hold() const;

private:
    ReplayContext *context_of(uint32_t context);
    void           replay_setup(const RecordHeader &record, const uint8_t *payload);
    void           replay_batch(const RecordHeader &record, const uint8_t *payload,
                                const RecordingFilter &filter);
    void           replay_reply(const RecordHeader &record, const uint8_t *payload);
    void           replay_blob(const RecordHeader &record, const uint8_t *payload);
    void           open_finding(TextBuffer &text, const RecordHeader &record) const;
    void           keep_finding(const TextBuffer &text);
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
    KeyedSlots<uint32_t, ReplayContext *> by_context_;
    GrowableArray<Finding>                findings_text_;

    friend struct RecordingReplayProbe;
};

}

#endif
