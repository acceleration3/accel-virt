#include "device/replay/recording_replay.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/execution/executor_library.hpp"
#include "device/recording/record_kind.hpp"
#include "device/replay/recorded_command.hpp"
#include "device/replay/recording_filter.hpp"
#include "device/recording/recording_header.hpp"
#include "device/replay/recording_reader.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

namespace {

constexpr uint32_t blob_imported = 1u;

}

RecordingReplay::RecordingReplay(ExecutorLibrary &library, BlockSource &memory)
    : library_(library),
      gpu_(),
      gpu_chosen_(false),
      ran_(false),
      contexts_(0u),
      commands_(0u),
      replies_(0u),
      findings_(0u),
      replays_(memory),
      by_context_(memory, Pool::paged),
      findings_text_(memory, Pool::paged)
{
    AV_SCOPE("RecordingReplay::RecordingReplay");
}

RecordingReplay::~RecordingReplay()
{
    AV_SCOPE("RecordingReplay::~RecordingReplay");
    by_context_.for_each([this](uint32_t, ReplayContext *replay) { replay->destroy(library_); });
    library_.close_domain();
}

void RecordingReplay::use_gpu(const uint8_t *uuid)
{
    AV_SCOPE("RecordingReplay::use_gpu");
    if (uuid == nullptr) return;
    gpu_ = DeviceUuid(uuid);
    gpu_chosen_ = true;
}

Status RecordingReplay::run(RecordingReader &reader, const RecordingFilter &filter)
{
    AV_SCOPE("RecordingReplay::run");
    if (ran_) return Status::invalid_state;
    ran_ = true;
    const Status opened = reader.open();
    if (opened != Status::ok) return Status::malformed;
    if (!gpu_chosen_) use_gpu(reader.header().uuid());
    static_cast<void>(library_.open_domain(gpu_.raw(), 0u, 0u, 0u));
    RecordHeader   record;
    const uint8_t *payload = nullptr;
    for (;;) {
        const Status status = reader.next(record, payload);
        if (status == Status::not_found) return Status::ok;
        if (status != Status::ok) return Status::malformed;
        if (!filter.keeps(record)) continue;
        switch (record.kind()) {
        case RecordKind::context_setup: replay_setup(record, payload); break;
        case RecordKind::batch: replay_batch(record, payload, filter); break;
        case RecordKind::reply: replay_reply(record, payload); break;
        case RecordKind::blob: replay_blob(record, payload); break;
        default: break;
        }
    }
}

ReplayContext *RecordingReplay::context_of(uint32_t context)
{
    AV_SCOPE("RecordingReplay::context_of");
    ReplayContext *const *const replay = by_context_.find(context);
    return replay != nullptr ? *replay : nullptr;
}

void RecordingReplay::replay_setup(const RecordHeader &record, const uint8_t *payload)
{
    AV_SCOPE("RecordingReplay::replay_setup");
    ContextSetup setup;
    if (setup.decode(payload, record.length()) != Status::ok) {
        finding_about(record, "its setup does not decode");
        return;
    }
    ReplayContext *replay = context_of(record.context());
    if (replay != nullptr) {
        replay->destroy(library_);
    } else {
        uint32_t slot = 0u;
        if (by_context_.reserve(by_context_.count() + 1u) != Status::ok || replays_.create(replay) != Status::ok) {
            finding_about(record, "there was no memory to replay it in");
            return;
        }
        static_cast<void>(by_context_.insert(record.context(), replay, slot));
    }
    if (replay->create(library_, record.context(), setup.sub_api()) != Status::ok) {
        finding_about(record, "the library made no executor for it");
        static_cast<void>(by_context_.erase(record.context()));
        static_cast<void>(replays_.destroy(replay));
        return;
    }
    contexts_++;
}

void RecordingReplay::replay_batch(const RecordHeader &record, const uint8_t *payload,
                                   const RecordingFilter &filter)
{
    AV_SCOPE("RecordingReplay::replay_batch");
    ReplayContext *const context = context_of(record.context());
    if (context == nullptr) {
        finding_about(record, "a batch of a context that was never set up");
        return;
    }
    if (context->gave_up()) return;
    context->forget_answer();
    RecordedCommand command;
    uint32_t        offset = 0u;
    uint64_t        index = 0u;
    while (offset < record.length()) {
        if (command.decode(payload, record.length(), offset) != Status::ok) {
            char       bytes[finding_bytes];
            TextBuffer text(bytes, sizeof bytes);
            open_finding(text, record);
            text.append("the commands are framed wrong from byte ");
            text.append_decimal(offset);
            keep_finding(text);
            context->give_up();
            return;
        }
        offset = command.next_offset();
        index++;
        if (command.header().api() == CommandHeader::transport_api) {
            const uint32_t id = command.header().id();
            if (id == CommandHeader::answer_piece_id) context->answer_in_parts();
            if (id != CommandHeader::import_blob_id && id != CommandHeader::piece_id) continue;
            char       bytes[finding_bytes];
            TextBuffer text(bytes, sizeof bytes);
            open_finding(text, record);
            text.append("unreplayable: command ");
            text.append_decimal(index);
            text.append(id == CommandHeader::piece_id ? " is part of a command too long for one batch"
                                                      : " imports guest memory");
            keep_finding(text);
            context->give_up();
            return;
        }
        if (command.header().api() != context->command_api() || !filter.keeps(command)) continue;
        const Status ran = context->execute(library_, command);
        commands_++;
        if (ran == Status::not_found) continue;
        if (ran == Status::ok) continue;
        char       bytes[finding_bytes];
        TextBuffer text(bytes, sizeof bytes);
        open_finding(text, record);
        text.append(ran == Status::malformed ? "the library could not decode command "
                                             : "the library lost its device at command ");
        text.append_decimal(index);
        text.append(" ");
        text.append(command.name());
        keep_finding(text);
        context->give_up();
        return;
    }
}

void RecordingReplay::replay_reply(const RecordHeader &record, const uint8_t *payload)
{
    AV_SCOPE("RecordingReplay::replay_reply");
    ReplayContext *const context = context_of(record.context());
    if (context == nullptr || context->gave_up()) return;
    replies_++;
    if (context->answers_with(payload, record.length())) return;
    char       bytes[finding_bytes];
    TextBuffer text(bytes, sizeof bytes);
    open_finding(text, record);
    text.append("replied ");
    text.append_decimal(context->answered() ? context->answer_length() : 0u);
    text.append(" bytes, recorded ");
    text.append_decimal(record.length());
    keep_finding(text);
}

void RecordingReplay::replay_blob(const RecordHeader &record, const uint8_t *payload)
{
    AV_SCOPE("RecordingReplay::replay_blob");
    WireReader     reader(payload, record.length());
    const uint32_t action = reader.read_u32();
    if (reader.status() != Status::ok || action != blob_imported) return;
    ReplayContext *const context = context_of(record.context());
    if (context == nullptr || context->gave_up()) return;
    finding_about(record, "unreplayable: the recording holds no bytes for the blob it imported");
    context->give_up();
}

void RecordingReplay::open_finding(TextBuffer &text, const RecordHeader &record) const
{
    AV_SCOPE("RecordingReplay::open_finding");
    text.append("ctx=");
    text.append_decimal(record.context());
    text.append(" fence=");
    text.append_decimal(record.fence());
    text.append(" ");
}

void RecordingReplay::keep_finding(const TextBuffer &text)
{
    AV_SCOPE("RecordingReplay::keep_finding");
    findings_++;
    Finding kept = {};
    for (size_t offset = 0u; offset < text.length(); offset++)
        kept.text_[offset] = text.text()[offset];
    static_cast<void>(findings_text_.append(kept));
}

void RecordingReplay::finding_about(const RecordHeader &record, const char *what)
{
    AV_SCOPE("RecordingReplay::finding_about");
    char       bytes[finding_bytes];
    TextBuffer text(bytes, sizeof bytes);
    open_finding(text, record);
    text.append(what);
    keep_finding(text);
}

uint32_t RecordingReplay::contexts() const
{
    AV_SCOPE("RecordingReplay::contexts");
    return contexts_;
}

uint64_t RecordingReplay::commands() const
{
    AV_SCOPE("RecordingReplay::commands");
    return commands_;
}

uint64_t RecordingReplay::replies() const
{
    AV_SCOPE("RecordingReplay::replies");
    return replies_;
}

uint32_t RecordingReplay::findings() const
{
    AV_SCOPE("RecordingReplay::findings");
    return findings_;
}

const char *RecordingReplay::finding(uint32_t index) const
{
    AV_SCOPE("RecordingReplay::finding");
    if (index >= findings_text_.size()) return "";
    return findings_text_.at(index).text_;
}

bool RecordingReplay::invariants_hold() const
{
    AV_SCOPE("RecordingReplay::invariants_hold");
    if (!replays_.all_of([](const ReplayContext &replay) { return replay.invariants_hold() && replay.created(); }))
        return false;
    for (const Finding &kept : findings_text_)
        if (kept.text_[finding_bytes - 1u] != '\0') return false;
    return by_context_.count() == replays_.live() && by_context_.count() <= contexts_ &&
           findings_text_.size() <= findings_;
}

}
