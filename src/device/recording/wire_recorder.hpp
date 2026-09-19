/* Writes down what crosses one device's wire (docs/protocol.md): the file
 * header, then a record per context setup, batch, reply, completed fence,
 * fault and blob, each appended and flushed as it is taken, so a QEMU that
 * dies inside a host driver leaves behind the batch it was running.
 *
 * One recorder per device, written by every context's worker: a mutex it owns
 * keeps a record whole and keeps records in the order the device took them.
 * Without a file every record call returns on its first line, which is what
 * "recording is off by default and the submission path is unchanged" means. */
#ifndef AV_WIRE_RECORDER_HPP
#define AV_WIRE_RECORDER_HPP

#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/record_kinds.hpp"
#include "device/recording/recording_header.hpp"
#include "device/recording/recording_limit.hpp"
#include "proto/submission/fault_code.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextSetup;
class GpuIdentity;
class Host;
class ProtocolVersion;

/* What happened to a blob, as the blob record's action field. */
enum class BlobAction : uint32_t { created = 0u, imported = 1u, released = 2u };

struct WireRecorderProbe;

class WireRecorder {
public:
    /* The fixed payloads are all smaller than this, so one buffer carries a
     * record's header and its whole fixed part. */
    static constexpr size_t frame_bytes = RecordHeader::encoded_size + 32u;
    static constexpr size_t blob_payload = 32u;
    static constexpr size_t blob_bytes_prefix = 16u;

    explicit WireRecorder(Host &host);
    ~WireRecorder();
    WireRecorder(const WireRecorder &) = delete;
    WireRecorder &operator=(const WireRecorder &) = delete;

    /* Main loop. A null or empty `path` leaves recording off and answers ok: a
     * run is never refused for the sake of a recording, and a host that cannot
     * make the file says so once in the device log. invalid_parameter for
     * `kinds` that name nothing; invalid_state once recording. */
    [[nodiscard]] Status open(const char *path, uint64_t limit_bytes, const char *kinds, AtCeiling at_ceiling,
                              const GpuIdentity &gpu, const ProtocolVersion &wire);
    /* Main loop, once no worker can still be recording. */
    void                 close();
    bool                 recording() const;

    /* Any thread. Each returns at once when the kind is not kept. */
    void record_context_setup(uint32_t context, const ContextSetup &setup);
    void record_batch(uint32_t context, uint64_t fence, const uint8_t *payload, uint32_t length);
    void record_reply(uint32_t context, uint64_t fence, const uint8_t *answer, uint32_t length);
    void record_fence(uint32_t context, uint64_t fence);
    void record_fault(uint32_t context, uint64_t fence, FaultCode fault);
    void record_blob(uint32_t context, BlobAction action, uint32_t memory_type, uint64_t blob_id,
                     uint64_t offset, uint64_t size);
    /* The bytes an import handed the executor, when `blob_bytes` is kept. A
     * range larger than a record may hold is counted as refused, once. */
    void record_blob_bytes(uint32_t context, uint64_t blob_id, uint64_t offset, const uint8_t *bytes,
                           uint64_t size);

    uint64_t records() const;
    /* Records turned away while the recording still ran: the ceiling reached,
     * or a record larger than one may be. A write the host refuses ends the
     * recording instead, which the device log says once. */
    uint64_t refused() const;
    uint32_t wraps() const;

    bool invariants_hold() const;

private:
    /* The record's `fixed_length` bytes at `fixed` and `bulk_length` at
     * `bulk`, framed and appended under the lock. */
    void                 write(RecordKind kind, uint32_t context, uint64_t fence, const uint8_t *fixed,
                               uint32_t fixed_length, const uint8_t *bulk, uint64_t bulk_length);
    /* Under the lock: one more record the recording could not take. */
    void                 refuse();
    /* Under the lock: the file header at the start of an empty file. */
    [[nodiscard]] Status write_file_header();
    /* Under the lock: stops recording and says why, once. */
    void                 give_up(const char *reason);

    Host           &host_;
    void           *file_;
    void           *lock_;
    RecordingHeader header_;
    RecordingLimit  limit_;
    RecordKinds     kinds_;
    uint64_t        started_;
    uint64_t        records_;
    uint64_t        refused_;
    bool            full_;
    uint8_t         frame_[frame_bytes];

    friend struct WireRecorderProbe;
};

}  // namespace av

#endif  // AV_WIRE_RECORDER_HPP
