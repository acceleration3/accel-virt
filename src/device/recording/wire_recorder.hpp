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

enum class BlobAction : uint32_t { created = 0u, imported = 1u, released = 2u };

struct WireRecorderProbe;

class WireRecorder {
public:
    static constexpr size_t frame_bytes = RecordHeader::encoded_size + 32u;
    static constexpr size_t blob_payload = 32u;
    static constexpr size_t blob_bytes_prefix = 16u;

    explicit WireRecorder(Host &host);
    ~WireRecorder();
    WireRecorder(const WireRecorder &) = delete;
    WireRecorder &operator=(const WireRecorder &) = delete;

    [[nodiscard]] Status open(const char *path, uint64_t limit_bytes, const char *kinds, AtCeiling at_ceiling,
                              const GpuIdentity &gpu, const ProtocolVersion &wire);
    void                 close();
    bool                 recording() const;

    void record_context_setup(uint32_t context, const ContextSetup &setup);
    void record_batch(uint32_t context, uint64_t fence, const uint8_t *payload, uint32_t length);
    void record_reply(uint32_t context, uint64_t fence, const uint8_t *answer, uint32_t length);
    void record_fence(uint32_t context, uint64_t fence);
    void record_fault(uint32_t context, uint64_t fence, FaultCode fault);
    void record_blob(uint32_t context, BlobAction action, uint32_t memory_type, uint64_t blob_id,
                     uint64_t offset, uint64_t size);
    void record_blob_bytes(uint32_t context, uint64_t blob_id, uint64_t offset, const uint8_t *bytes,
                           uint64_t size);

    uint64_t records() const;
    uint64_t refused() const;
    uint32_t wraps() const;

    bool invariants_hold() const;

private:
    void                 write(RecordKind kind, uint32_t context, uint64_t fence, const uint8_t *fixed,
                               uint32_t fixed_length, const uint8_t *bulk, uint64_t bulk_length);
    void                 refuse();
    [[nodiscard]] Status write_file_header();
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

}

#endif
