#include "device/recording/wire_recorder.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

WireRecorder::WireRecorder(Host &host)
    : host_(host),
      file_(nullptr),
      lock_(nullptr),
      header_(),
      limit_(),
      kinds_(),
      started_(0u),
      records_(0u),
      refused_(0u),
      full_(false),
      frame_{}
{
    AV_SCOPE("WireRecorder::WireRecorder");
}

WireRecorder::~WireRecorder()
{
    AV_SCOPE("WireRecorder::~WireRecorder");
    close();
}

Status WireRecorder::open(const char *path, uint64_t limit_bytes, const char *kinds, AtCeiling at_ceiling,
                          const GpuIdentity &gpu, const ProtocolVersion &wire)
{
    AV_SCOPE("WireRecorder::open");
    host_.require_main_loop("WireRecorder::open");
    if (file_ != nullptr) return Status::invalid_state;
    RecordKinds wanted;
    const Status named = wanted.parse(kinds);
    if (named != Status::ok) return named;
    if (limit_bytes != 0u && limit_bytes < RecordingHeader::encoded_size) return Status::invalid_parameter;
    if (path == nullptr || path[0] == '\0') return Status::ok;
    if (host_.create_mutex(lock_) != Status::ok) {
        host_.log("recording: no mutex for the recording; recording is off");
        return Status::ok;
    }
    if (host_.create_recording(path, file_) != Status::ok) {
        host_.destroy_mutex(lock_);
        lock_ = nullptr;
        file_ = nullptr;
        host_.log("recording: the recording file could not be made; recording is off");
        return Status::ok;
    }
    kinds_ = wanted;
    started_ = host_.monotonic_nanoseconds();
    limit_ = RecordingLimit(limit_bytes != 0u ? limit_bytes : RecordingLimit::default_ceiling, at_ceiling);
    header_ = RecordingHeader(wire, kinds_.mask(), started_, limit_.ceiling(), gpu.vendor_id(),
                              gpu.device_id(), gpu.uuid());
    records_ = 0u;
    refused_ = 0u;
    full_ = false;
    if (write_file_header() != Status::ok) give_up("recording: the file header could not be written");
    return Status::ok;
}

void WireRecorder::close()
{
    AV_SCOPE("WireRecorder::close");
    if (file_ != nullptr) host_.close_recording(file_);
    file_ = nullptr;
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
    lock_ = nullptr;
}

/* What a caller asks; `write` asks `full_` again under the lock, which is the
 * answer that decides. */
bool WireRecorder::recording() const
{
    AV_SCOPE("WireRecorder::recording");
    return file_ != nullptr && !full_;
}

Status WireRecorder::write_file_header()
{
    AV_SCOPE("WireRecorder::write_file_header");
    uint8_t bytes[RecordingHeader::encoded_size] = {};
    static_cast<void>(header_.encode(bytes, sizeof bytes)); /* sized for it, and open refused a smaller ceiling */
    const Status written = host_.append_recording(file_, bytes, sizeof bytes);
    if (written != Status::ok) return written;
    limit_.wrapped();
    return Status::ok;
}

/* Under the lock, and only from a path that has just seen `full_` false, so
 * the reason is logged exactly once for a recording. */
void WireRecorder::give_up(const char *reason)
{
    AV_SCOPE("WireRecorder::give_up");
    full_ = true;
    host_.log(reason);
}

void WireRecorder::refuse()
{
    AV_SCOPE("WireRecorder::refuse");
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    refused_++;
}

void WireRecorder::write(RecordKind kind, uint32_t context, uint64_t fence, const uint8_t *fixed,
                         uint32_t fixed_length, const uint8_t *bulk, uint64_t bulk_length)
{
    AV_SCOPE("WireRecorder::write");
    const uint64_t length = uint64_t{fixed_length} + bulk_length;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    if (full_) return;
    if (length > RecordHeader::max_payload) {
        refused_++;
        return;
    }
    const uint64_t taken = host_.monotonic_nanoseconds() - started_;
    const RecordHeader header(kind, context, static_cast<uint32_t>(length), taken, fence);
    const LimitVerdict verdict = limit_.verdict(header.span());
    if (verdict == LimitVerdict::stop) {
        refused_++;
        give_up("recording: the size limit was reached; nothing more is recorded");
        return;
    }
    if (verdict == LimitVerdict::wrap) {
        if (host_.truncate_recording(file_) != Status::ok) {
            give_up("recording: the recording could not be wrapped; nothing more is recorded");
            return;
        }
        header_.wrapped();
        if (write_file_header() != Status::ok) {
            give_up("recording: the file header could not be rewritten; nothing more is recorded");
            return;
        }
        host_.log("recording: the size limit was reached; the recording wrapped");
    }
    static_cast<void>(header.encode(frame_, sizeof frame_)); /* sized for it, and the length was judged above */
    for (uint32_t index = 0u; index < fixed_length; index++)
        frame_[RecordHeader::encoded_size + index] = fixed[index];
    if (host_.append_recording(file_, frame_, RecordHeader::encoded_size + fixed_length) != Status::ok) {
        give_up("recording: the recording could not be written; nothing more is recorded");
        return;
    }
    if (bulk_length != 0u && host_.append_recording(file_, bulk, bulk_length) != Status::ok) {
        give_up("recording: the recording could not be written; nothing more is recorded");
        return;
    }
    limit_.took(header.span());
    records_++;
}

void WireRecorder::record_context_setup(uint32_t context, const ContextSetup &setup)
{
    AV_SCOPE("WireRecorder::record_context_setup");
    if (file_ == nullptr || !kinds_.keeps(RecordKind::context_setup)) return;
    uint8_t bytes[ContextSetup::encoded_size] = {};
    if (setup.encode(bytes, sizeof bytes) != Status::ok) {
        refuse();
        return;
    }
    write(RecordKind::context_setup, context, 0u, nullptr, 0u, bytes, setup.size());
}

void WireRecorder::record_batch(uint32_t context, uint64_t fence, const uint8_t *payload, uint32_t length)
{
    AV_SCOPE("WireRecorder::record_batch");
    if (file_ == nullptr || !kinds_.keeps(RecordKind::batch)) return;
    write(RecordKind::batch, context, fence, nullptr, 0u, payload, length);
}

void WireRecorder::record_reply(uint32_t context, uint64_t fence, const uint8_t *answer, uint32_t length)
{
    AV_SCOPE("WireRecorder::record_reply");
    if (file_ == nullptr || !kinds_.keeps(RecordKind::reply)) return;
    write(RecordKind::reply, context, fence, nullptr, 0u, answer, length);
}

void WireRecorder::record_fence(uint32_t context, uint64_t fence)
{
    AV_SCOPE("WireRecorder::record_fence");
    if (file_ == nullptr || !kinds_.keeps(RecordKind::fence_completed)) return;
    write(RecordKind::fence_completed, context, fence, nullptr, 0u, nullptr, 0u);
}

void WireRecorder::record_fault(uint32_t context, uint64_t fence, FaultCode fault)
{
    AV_SCOPE("WireRecorder::record_fault");
    if (file_ == nullptr || !kinds_.keeps(RecordKind::fault)) return;
    uint8_t    bytes[8] = {};
    WireWriter writer(bytes, sizeof bytes);
    writer.write_u32(static_cast<uint32_t>(fault));
    writer.write_u32(0u);
    write(RecordKind::fault, context, fence, bytes, sizeof bytes, nullptr, 0u);
}

void WireRecorder::record_blob(uint32_t context, BlobAction action, uint32_t memory_type, uint64_t blob_id,
                               uint64_t offset, uint64_t size)
{
    AV_SCOPE("WireRecorder::record_blob");
    if (file_ == nullptr || !kinds_.keeps(RecordKind::blob)) return;
    uint8_t    bytes[blob_payload] = {};
    WireWriter writer(bytes, sizeof bytes);
    writer.write_u32(static_cast<uint32_t>(action));
    writer.write_u32(memory_type);
    writer.write_u64(blob_id);
    writer.write_u64(offset);
    writer.write_u64(size);
    write(RecordKind::blob, context, 0u, bytes, sizeof bytes, nullptr, 0u);
}

void WireRecorder::record_blob_bytes(uint32_t context, uint64_t blob_id, uint64_t offset,
                                     const uint8_t *bytes, uint64_t size)
{
    AV_SCOPE("WireRecorder::record_blob_bytes");
    if (file_ == nullptr || !kinds_.keeps(RecordKind::blob_bytes)) return;
    uint8_t    prefix[blob_bytes_prefix] = {};
    WireWriter writer(prefix, sizeof prefix);
    writer.write_u64(blob_id);
    writer.write_u64(offset);
    write(RecordKind::blob_bytes, context, 0u, prefix, sizeof prefix, bytes, size);
}

uint64_t WireRecorder::records() const
{
    AV_SCOPE("WireRecorder::records");
    return records_;
}

uint64_t WireRecorder::refused() const
{
    AV_SCOPE("WireRecorder::refused");
    return refused_;
}

uint32_t WireRecorder::wraps() const
{
    AV_SCOPE("WireRecorder::wraps");
    return header_.wraps();
}

bool WireRecorder::invariants_hold() const
{
    AV_SCOPE("WireRecorder::invariants_hold");
    if (file_ == nullptr) return lock_ == nullptr && records_ == 0u;
    /* The kinds are not asked: they are parsed once at open and never touched
     * again, so the recorder has nothing of its own to keep true about them. */
    return lock_ != nullptr && header_.invariants_hold() && limit_.invariants_hold();
}

}  // namespace av
