#include "guest/inspect/inspect_program.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/trace_layout.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "guest/inspect/escape_reader.hpp"
#include "guest/inspect/inspect_host.hpp"
#include "guest/inspect/snapshot_file.hpp"

namespace av {

namespace {

constexpr size_t   name_bytes = 64u;
constexpr size_t   path_bytes = 520u;
constexpr size_t   reason_bytes = 128u;
constexpr uint32_t state_length_bytes = 4u;
/* The modules whose tables `list` looks for (docs/trace.md). */
constexpr const char *listed_modules[] = {"avvk", "avgl", "avtest"};

/* A process makes its objects in the global namespace when it may, and in its
 * session's otherwise, so a reader tries them in that order. */
struct ObjectScope {
    const char *prefix;
    const char *label;
};
constexpr ObjectScope object_scopes[] = {{"Global\\", "global"}, {"Local\\", "local"}};

}  // namespace

InspectProgram::InspectProgram(InspectHost &host, Kmt &kmt, BlockSource &memory)
    : host_(host), kmt_(kmt), memory_(memory), line_()
{
    AV_SCOPE("InspectProgram::InspectProgram");
}

/* Every command ends with `--out <path>`, so the path is always the last
 * argument and the command's own arguments are what precedes it. */
InspectProgram::Exit InspectProgram::run(const uint16_t *line)
{
    AV_SCOPE("InspectProgram::run");
    if (line_.parse(line) != Status::ok || line_.count() < 3u) return Exit::usage;
    const uint32_t     count = line_.count();
    const char *const  out = line_.argument(count - 1u);
    const char *const  command = line_.argument(0u);
    if (__builtin_strcmp(line_.argument(count - 2u), "--out") != 0) return Exit::usage;
    uint32_t pid = 0u;
    if (count == 3u && __builtin_strcmp(command, "list") == 0) return list(out);
    /* The kernel's commands take the avgpu adapter's LUID; the first adapter
     * when it is left out. */
    uint32_t luid = 0u;
    if ((count == 3u || (count == 4u && line_.number(1u, luid))) && __builtin_strcmp(command, "kernel-trace") == 0)
        return kernel(TraceEscapeRecord::Kind::table, luid, out);
    if ((count == 3u || (count == 4u && line_.number(1u, luid))) && __builtin_strcmp(command, "kernel-state") == 0)
        return kernel(TraceEscapeRecord::Kind::state, luid, out);
    if (count != 5u || !line_.number(1u, pid) || !module_name(line_.argument(2u))) return Exit::usage;
    if (__builtin_strcmp(command, "trace") == 0) return trace(pid, line_.argument(2u), out);
    if (__builtin_strcmp(command, "state") == 0) return state(pid, line_.argument(2u), out);
    return Exit::usage;
}

const char *InspectProgram::usage() const
{
    AV_SCOPE("InspectProgram::usage");
    return "usage: avinspect list --out FILE\n"
           "       avinspect kernel-trace|kernel-state [LUID] --out FILE\n"
           "       avinspect trace|state PID MODULE --out FILE\n";
}

InspectProgram::Exit InspectProgram::list(const char *out)
{
    AV_SCOPE("InspectProgram::list");
    auto *const text = static_cast<char *>(memory_.allocate(list_bytes, Pool::paged));
    auto *const pids = static_cast<uint32_t *>(memory_.allocate(sizeof(uint32_t) * max_processes, Pool::paged));
    ScopeExit   release([&] {
        if (text != nullptr) memory_.release(text);
        if (pids != nullptr) memory_.release(pids);
    });
    if (text == nullptr || pids == nullptr) return Exit::unwritten;
    TextBuffer     lines(text, list_bytes);
    const uint32_t found = host_.processes(pids, max_processes);
    for (uint32_t index = 0u; index < found && index < max_processes; index++) {
        for (const char *const module : listed_modules) {
            for (const ObjectScope &scope : object_scopes) {
                char       object[name_bytes];
                TextBuffer section(object, sizeof object);
                name(section, scope.prefix, "trace", module, pids[index]);
                uint8_t header[TraceLayout::header_bytes];
                size_t  copied = 0u;
                if (host_.copy_section(object, header, sizeof header, copied) != Status::ok) continue;
                char path[path_bytes] = {};
                if (host_.executable(pids[index], path, sizeof path) != Status::ok) path[0] = '\0';
                lines.append("table pid=");
                lines.append_decimal(pids[index]);
                lines.append(" module=");
                lines.append(module);
                lines.append(" scope=");
                lines.append(scope.label);
                lines.append(" exe=");
                lines.append(path);
                lines.append("\n");
                break;
            }
        }
    }
    return finish(host_.write_file(out, nullptr, 0u, static_cast<const uint8_t *>(static_cast<const void *>(text)),
                                   lines.length()));
}

InspectProgram::Exit InspectProgram::trace(uint32_t pid, const char *module, const char *out)
{
    AV_SCOPE("InspectProgram::trace");
    const size_t capacity = TraceLayout(static_cast<uint32_t>(TraceLayout::max_slots)).bytes();
    auto *const  copy = static_cast<uint8_t *>(memory_.allocate(capacity, Pool::paged));
    if (copy == nullptr) return Exit::unwritten;
    ScopeExit release([&] { memory_.release(copy); });
    for (const ObjectScope &scope : object_scopes) {
        char       object[name_bytes];
        TextBuffer section(object, sizeof object);
        name(section, scope.prefix, "trace", module, pid);
        size_t copied = 0u;
        if (host_.copy_section(object, copy, capacity, copied) != Status::ok) continue;
        return table_file(out, host_.clock(), copy, copied, object);
    }
    char       reason[reason_bytes];
    TextBuffer text(reason, sizeof reason);
    text.append("no trace table for ");
    text.append(module);
    text.append(" in process ");
    text.append_decimal(pid);
    return finish(SnapshotFile(host_, out).refuse(host_.clock(), text.text()));
}

InspectProgram::Exit InspectProgram::state(uint32_t pid, const char *module, const char *out)
{
    AV_SCOPE("InspectProgram::state");
    SnapshotFile file(host_, out);
    for (const ObjectScope &scope : object_scopes) {
        char       dump[name_bytes];
        char       done[name_bytes];
        TextBuffer dump_name(dump, sizeof dump);
        TextBuffer done_name(done, sizeof done);
        name(dump_name, scope.prefix, "dump", module, pid);
        name(done_name, scope.prefix, "done", module, pid);
        const Status asked = host_.request_state(dump, done, state_timeout_ms);
        if (asked == Status::not_found) continue;
        if (asked != Status::ok) return finish(file.refuse(host_.clock(), "the state service did not answer in 5 s"));
        char       answer[name_bytes];
        TextBuffer answer_name(answer, sizeof answer);
        name(answer_name, scope.prefix, "state", module, pid);
        return dump_file(file, answer);
    }
    return finish(file.refuse(host_.clock(), "no state service in that process"));
}

/* The service's section: a u32 length, then that much text. */
InspectProgram::Exit InspectProgram::dump_file(SnapshotFile &file, const char *section)
{
    AV_SCOPE("InspectProgram::dump_file");
    auto *const bytes = static_cast<uint8_t *>(memory_.allocate(state_bytes, Pool::paged));
    if (bytes == nullptr) return Exit::unwritten;
    ScopeExit release([&] { memory_.release(bytes); });
    size_t    copied = 0u;
    if (host_.copy_section(section, bytes, state_bytes, copied) != Status::ok)
        return finish(file.refuse(host_.clock(), "the state service answered but its dump cannot be opened"));
    uint32_t length = 0u;
    for (uint32_t byte = 0u; byte < state_length_bytes && byte < copied; byte++)
        length |= static_cast<uint32_t>(bytes[byte]) << (8u * byte);
    if (copied < state_length_bytes || length > copied - state_length_bytes)
        return finish(file.refuse(host_.clock(), "the state dump is longer than its section"));
    return finish(file.write(SnapshotFile::Kind::state, host_.clock(), bytes + state_length_bytes, length));
}

InspectProgram::Exit InspectProgram::kernel(TraceEscapeRecord::Kind kind, uint64_t luid, const char *out)
{
    AV_SCOPE("InspectProgram::kernel");
    EscapeReader reader(kmt_, memory_);
    const Status read = reader.read(kind, luid);
    SnapshotFile file(host_, out);
    if (read != Status::ok) {
        char       reason[reason_bytes];
        TextBuffer text(reason, sizeof reason);
        text.append(read == Status::not_found ? "no avgpu adapter" : "the driver's escape failed, status ");
        if (read != Status::not_found) text.append_decimal(static_cast<uint32_t>(read));
        return finish(file.refuse(host_.clock(), text.text()));
    }
    if (kind == TraceEscapeRecord::Kind::table)
        return table_file(out, reader.clock(), reader.bytes(), reader.length(), "the driver");
    return finish(file.write(SnapshotFile::Kind::state, reader.clock(), reader.bytes(), reader.length()));
}

void InspectProgram::name(TextBuffer &out, const char *prefix, const char *kind, const char *module,
                          uint32_t pid) const
{
    AV_SCOPE("InspectProgram::name");
    out.append(prefix);
    out.append("av-");
    out.append(kind);
    out.append("-");
    out.append(module);
    out.append("-");
    out.append_decimal(pid);
}

/* A module name goes into an object name: lower-case letters and digits only. */
bool InspectProgram::module_name(const char *module) const
{
    AV_SCOPE("InspectProgram::module_name");
    size_t length = 0u;
    for (; module[length] != '\0'; length++) {
        const char letter = module[length];
        if (length == max_module || !((letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9')))
            return false;
    }
    return length != 0u;
}

InspectProgram::Exit InspectProgram::table_file(const char *out, uint64_t clock, const uint8_t *bytes, size_t length,
                                                const char *source)
{
    AV_SCOPE("InspectProgram::table_file");
    SnapshotFile  file(host_, out);
    TraceSnapshot snapshot;
    if (snapshot.read(bytes, length) != Status::ok) {
        char       reason[reason_bytes];
        TextBuffer text(reason, sizeof reason);
        text.append("the table from ");
        text.append(source);
        text.append(" is malformed");
        return finish(file.refuse(clock, text.text()));
    }
    return finish(file.write(SnapshotFile::Kind::table, clock, bytes,
                             TraceLayout(snapshot.slots()).bytes()));
}

InspectProgram::Exit InspectProgram::finish(Status written) const
{
    AV_SCOPE("InspectProgram::finish");
    return written == Status::ok ? Exit::written : Exit::unwritten;
}

}  // namespace av
