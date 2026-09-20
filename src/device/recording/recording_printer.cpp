#include "device/recording/recording_printer.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recording_filter.hpp"
#include "device/recording/recording_reader.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

namespace {

constexpr const char *fault_names[] = {"none",        "ring_header",     "ring_tail",   "batch_header",
                                       "batch_order", "command_framing", "undecodable", "device_lost",
                                       "no_executor", "broken_executor"};
constexpr uint32_t    fault_name_count = sizeof fault_names / sizeof fault_names[0];
constexpr const char *blob_actions[] = {"created", "imported", "released"};
constexpr uint32_t    blob_action_count = sizeof blob_actions / sizeof blob_actions[0];
constexpr const char *command_indent = "                   ";

/* SubApi's bits against the API half of the command ids that sub-API sends
 * (docs/protocol.md, Execution). */
class SubApiName {
public:
    constexpr SubApiName(uint32_t sub_api, uint16_t api, const char *text)
        : sub_api_(sub_api), api_(api), text_(text)
    {}

    constexpr bool is(uint32_t sub_api) const { return sub_api_ == sub_api; }
    constexpr uint16_t api() const { return api_; }
    constexpr const char *text() const { return text_; }

private:
    uint32_t    sub_api_;
    uint16_t    api_;
    const char *text_;
};

constexpr SubApiName sub_apis[] = {SubApiName(1u, 1u, "vulkan"), SubApiName(2u, 3u, "opengl"),
                                   SubApiName(4u, 2u, "opencl")};
constexpr uint32_t sub_api_count = sizeof sub_apis / sizeof sub_apis[0];

/* Microseconds with the nanoseconds under them, so a person reads the gaps. */
void append_time(TextBuffer &text, uint64_t nanoseconds)
{
    AV_SCOPE("append_time");
    text.append("+");
    text.append_decimal(nanoseconds / 1000u);
    text.append(".");
    const uint64_t rest = nanoseconds % 1000u;
    if (rest < 100u) text.append("0");
    if (rest < 10u) text.append("0");
    text.append_decimal(rest);
}

}  // namespace

RecordingPrinter::RecordingPrinter(RecordingReader &reader, const RecordingFilter &filter,
                                   uint32_t payload_bytes)
    : reader_(reader),
      filter_(filter),
      payload_bytes_(payload_bytes < most_payload_bytes ? payload_bytes : most_payload_bytes),
      stage_(Stage::unopened),
      record_(),
      payload_(nullptr),
      start_(0u),
      commands_(0u),
      command_offset_(0u),
      lines_(0u),
      damage_printed_(false),
      contexts_{},
      line_{}
{
    AV_SCOPE("RecordingPrinter::RecordingPrinter");
}

Status RecordingPrinter::next(const char *&line)
{
    AV_SCOPE("RecordingPrinter::next");
    line = nullptr;
    TextBuffer text(line_, sizeof line_);
    if (stage_ == Stage::unopened)
        stage_ = reader_.open() == Status::ok ? Stage::records : Stage::done;
    else if (stage_ == Stage::commands && next_command(text)) {
        line = text.text();
        lines_++;
        return Status::ok;
    }
    if (stage_ != Stage::done && take_record()) {
        append_prefix(text);
        append_record(text);
        line = text.text();
        lines_++;
        return Status::ok;
    }
    stage_ = Stage::done;
    /* The damage is the last line, once. */
    if (!reader_.damaged() || damage_printed_) return Status::not_found;
    damage_printed_ = true;
    append_damage(text);
    line = text.text();
    lines_++;
    return Status::ok;
}

bool RecordingPrinter::take_record()
{
    AV_SCOPE("RecordingPrinter::take_record");
    for (;;) {
        RecordHeader   found;
        const uint8_t *payload = nullptr;
        const uint64_t before = reader_.offset();
        if (reader_.next(found, payload) != Status::ok) return false;
        learn(found, payload);
        if (!filter_.keeps(found) || !shown(found, payload)) continue;
        record_ = found;
        payload_ = payload;
        start_ = before;
        stage_ = found.kind() == RecordKind::batch ? Stage::commands : Stage::records;
        command_offset_ = 0u;
        return true;
    }
}

void RecordingPrinter::learn(const RecordHeader &record, const uint8_t *payload)
{
    AV_SCOPE("RecordingPrinter::learn");
    if (record.kind() != RecordKind::context_setup) return;
    ContextSetup setup;
    if (setup.decode(payload, record.length()) != Status::ok) return;
    uint16_t api = ContextApi::unknown_api;
    for (uint32_t index = 0u; index < sub_api_count; index++)
        if (sub_apis[index].is(static_cast<uint32_t>(setup.sub_api()))) api = sub_apis[index].api();
    for (ContextApi &known : contexts_) {
        if (known.known() && !known.names(record.context())) continue;
        known.learn(record.context(), api);
        return;
    }
}

uint16_t RecordingPrinter::api_of(uint32_t context) const
{
    AV_SCOPE("RecordingPrinter::api_of");
    for (const ContextApi &known : contexts_)
        if (known.names(context)) return known.api();
    return RecordingFilter::any_api;
}

/* A batch is shown when one of its commands passes the filter; every other
 * kind passes on the sub-API of the context it names. Counting the commands
 * here is what lets the batch's own line say how many there are. */
bool RecordingPrinter::shown(const RecordHeader &record, const uint8_t *payload)
{
    AV_SCOPE("RecordingPrinter::shown");
    commands_ = 0u;
    if (record.kind() != RecordKind::batch) {
        if (filter_.names_a_command()) return false;
        return filter_.api() == RecordingFilter::any_api || filter_.api() == api_of(record.context());
    }
    bool            matched = false;
    RecordedCommand command;
    uint32_t        offset = 0u;
    while (offset < record.length() && command.decode(payload, record.length(), offset) == Status::ok) {
        commands_++;
        matched = matched || filter_.keeps(command);
        offset = command.next_offset();
    }
    return matched || (commands_ == 0u && filter_.api() == RecordingFilter::any_api
                       && !filter_.names_a_command());
}

bool RecordingPrinter::next_command(TextBuffer &text)
{
    AV_SCOPE("RecordingPrinter::next_command");
    RecordedCommand command;
    while (command_offset_ < record_.length()) {
        if (command.decode(payload_, record_.length(), command_offset_) != Status::ok) {
            text.append(command_indent);
            text.append("the bytes from ");
            text.append_decimal(command_offset_);
            text.append(" are framed wrong");
            command_offset_ = record_.length();
            return true;
        }
        command_offset_ = command.next_offset();
        if (!filter_.keeps(command)) continue;
        append_command(text, command);
        return true;
    }
    stage_ = Stage::records;
    return false;
}

void RecordingPrinter::append_prefix(TextBuffer &text) const
{
    AV_SCOPE("RecordingPrinter::append_prefix");
    text.append_hex(start_, 8u);
    text.append(" ");
    append_time(text, record_.timestamp());
    text.append(" ctx=");
    text.append_decimal(record_.context());
    if (record_.fence() != 0u) {
        text.append(" fence=");
        text.append_decimal(record_.fence());
    }
    text.append(" ");
}

void RecordingPrinter::append_record(TextBuffer &text) const
{
    AV_SCOPE("RecordingPrinter::append_record");
    switch (record_.kind()) {
    case RecordKind::context_setup: append_setup(text); return;
    case RecordKind::batch:
        text.append("batch ");
        text.append_decimal(record_.length());
        text.append(" bytes, ");
        text.append_decimal(commands_);
        text.append(" commands");
        return;
    case RecordKind::reply:
        text.append("reply ");
        text.append_decimal(record_.length());
        text.append(" bytes");
        return;
    case RecordKind::fence_completed: text.append("fence completed"); return;
    case RecordKind::fault: append_fault(text); return;
    case RecordKind::blob: append_blob(text); return;
    default: append_blob_bytes(text); return;
    }
}

void RecordingPrinter::append_setup(TextBuffer &text) const
{
    AV_SCOPE("RecordingPrinter::append_setup");
    ContextSetup setup;
    if (setup.decode(payload_, record_.length()) != Status::ok) {
        text.append("setup, unreadable");
        return;
    }
    text.append("setup api=");
    const char *named = "api";
    for (uint32_t index = 0u; index < sub_api_count; index++)
        if (sub_apis[index].is(static_cast<uint32_t>(setup.sub_api()))) named = sub_apis[index].text();
    text.append(named);
    text.append(" ring_order=");
    text.append_decimal(setup.ring_order());
    text.append(" doorbell=");
    text.append_decimal(setup.doorbell());
    text.append(" reply=");
    text.append_decimal(setup.reply_resource());
    text.append(" version=");
    text.append_decimal(setup.version().major_version());
    text.append(".");
    text.append_decimal(setup.version().minor_version());
}

void RecordingPrinter::append_fault(TextBuffer &text) const
{
    AV_SCOPE("RecordingPrinter::append_fault");
    WireReader     reader(payload_, record_.length());
    const uint32_t code = reader.read_u32();
    text.append("fault ");
    text.append_decimal(code);
    if (reader.status() != Status::ok || code >= fault_name_count) return;
    text.append(" ");
    text.append(fault_names[code]);
}

void RecordingPrinter::append_blob(TextBuffer &text) const
{
    AV_SCOPE("RecordingPrinter::append_blob");
    WireReader     reader(payload_, record_.length());
    const uint32_t action = reader.read_u32();
    const uint32_t memory_type = reader.read_u32();
    const uint64_t blob = reader.read_u64();
    const uint64_t offset = reader.read_u64();
    const uint64_t size = reader.read_u64();
    text.append("blob ");
    text.append(action < blob_action_count ? blob_actions[action] : "unknown");
    if (reader.status() != Status::ok) return;
    text.append(" id=");
    text.append_decimal(blob);
    text.append(" offset=");
    text.append_decimal(offset);
    text.append(" size=");
    text.append_decimal(size);
    text.append(" memory_type=");
    text.append_decimal(memory_type);
}

void RecordingPrinter::append_blob_bytes(TextBuffer &text) const
{
    AV_SCOPE("RecordingPrinter::append_blob_bytes");
    WireReader     reader(payload_, record_.length());
    const uint64_t blob = reader.read_u64();
    const uint64_t offset = reader.read_u64();
    text.append("blob bytes");
    if (reader.status() != Status::ok) return;
    text.append(" id=");
    text.append_decimal(blob);
    text.append(" offset=");
    text.append_decimal(offset);
    text.append(" ");
    text.append_decimal(record_.length() - 16u);
    text.append(" bytes");
}

void RecordingPrinter::append_command(TextBuffer &text, const RecordedCommand &command) const
{
    AV_SCOPE("RecordingPrinter::append_command");
    text.append(command_indent);
    text.append("0x");
    text.append_hex(command.header().id(), 8u);
    text.append(" ");
    text.append(command.name());
    text.append(" ");
    text.append_decimal(command.payload_length());
    text.append(" bytes");
    if (command.header().generates_reply()) text.append(" reply");
    append_transport(text, command);
    const uint32_t shown =
        command.payload_length() < payload_bytes_ ? command.payload_length() : payload_bytes_;
    if (shown == 0u) return;
    text.append(" ");
    for (uint32_t index = 0u; index < shown; index++) text.append_hex(command.payload()[index], 2u);
}

/* The transport's commands are this repository's own, so their arguments are
 * decoded in full; every other command's are its registry's (research.md §5). */
void RecordingPrinter::append_transport(TextBuffer &text, const RecordedCommand &command) const
{
    AV_SCOPE("RecordingPrinter::append_transport");
    if (command.header().api() != CommandHeader::transport_api) return;
    WireReader reader(command.payload(), command.payload_length());
    if (command.header().id() == CommandHeader::import_blob_id) {
        const uint32_t resource = reader.read_u32();
        reader.expect_zero(4u);
        const uint64_t offset = reader.read_u64();
        const uint64_t size = reader.read_u64();
        const uint32_t memory_type = reader.read_u32();
        if (reader.status() != Status::ok) return;
        text.append(" resource=");
        text.append_decimal(resource);
        text.append(" offset=");
        text.append_decimal(offset);
        text.append(" size=");
        text.append_decimal(size);
        text.append(" memory_type=");
        text.append_decimal(memory_type);
        return;
    }
    if (command.header().id() != CommandHeader::release_blob_id) return;
    const uint64_t memory = reader.read_u64();
    if (reader.status() != Status::ok) return;
    text.append(" memory=0x");
    text.append_hex(memory, 16u);
}

void RecordingPrinter::append_damage(TextBuffer &text) const
{
    AV_SCOPE("RecordingPrinter::append_damage");
    text.append("damaged at ");
    text.append_hex(reader_.damage_offset(), 8u);
    text.append(": ");
    text.append(reader_.damage());
}

uint64_t RecordingPrinter::lines() const
{
    AV_SCOPE("RecordingPrinter::lines");
    return lines_;
}

bool RecordingPrinter::damaged() const
{
    AV_SCOPE("RecordingPrinter::damaged");
    return reader_.damaged();
}

bool RecordingPrinter::invariants_hold() const
{
    AV_SCOPE("RecordingPrinter::invariants_hold");
    if (payload_bytes_ > most_payload_bytes) return false;
    if (stage_ == Stage::commands && payload_ == nullptr) return false;
    return command_offset_ <= record_.length() && filter_.invariants_hold();
}

}  // namespace av
