#include "data/McpTools.h"

#include "data/Model.h"
#include "text/Format.h"
#include "text/NumberConversion.h"
#include "threading/LockGuard.h"

namespace inspector {
namespace {

constexpr usize kMaxStateLines = 4000;

StringView StringArgument(const JsonValue& arguments, StringView key) {
    const JsonValue* value = arguments.IsObject() ? arguments.Find(key) : nullptr;
    if (value == nullptr) {
        return {};
    }
    const Optional<StringView> text = value->GetString();
    return text ? *text : StringView{};
}

f64 NumberArgument(const JsonValue& arguments, StringView key, f64 fallback) {
    const JsonValue* value = arguments.IsObject() ? arguments.Find(key) : nullptr;
    if (value == nullptr) {
        return fallback;
    }
    if (const Optional<f64> number = value->GetDouble()) {
        return *number;
    }
    if (const Optional<i64> number = value->GetInt()) {
        return static_cast<f64>(*number);
    }
    return fallback;
}

bool BoolArgument(const JsonValue& arguments, StringView key, bool fallback) {
    const JsonValue* value = arguments.IsObject() ? arguments.Find(key) : nullptr;
    if (value == nullptr) {
        return fallback;
    }
    return value->GetBool().ValueOr(fallback);
}

JsonValue Schema(InitializerList<Pair<StringView, StringView>> properties) {
    JsonValue schema = JsonValue::MakeObject();
    schema.Set("type", "object");
    JsonValue props = JsonValue::MakeObject();
    for (const Pair<StringView, StringView>& property : properties) {
        JsonValue entry = JsonValue::MakeObject();
        const usize colon = property.second.Find(':');
        entry.Set("type", property.second.Substr(0, colon));
        entry.Set("description", property.second.Substr(colon + 1));
        props.Set(property.first, RValueCast(entry));
    }
    schema.Set("properties", RValueCast(props));
    return schema;
}

JsonValue Rate(const Optional<f64>& rate) {
    return rate ? JsonValue(*rate) : JsonValue();
}

// "device/1240628", "1240628", or "device" (the first live process of that module).
const ProcessSample* ResolveProcess(const Sample& sample, StringView wanted) {
    if (const ProcessSample* exact = sample.Find(wanted)) {
        return exact;
    }
    const Optional<u32> pid = ParseInt<u32>(wanted);
    const ProcessSample* fallback = nullptr;
    for (const ProcessSample& process : sample.processes) {
        if (pid && process.source.pid == *pid) {
            return &process;
        }
        if (process.source.module.GetView() == wanted) {
            if (process.source.alive) {
                return &process;
            }
            fallback = fallback != nullptr ? fallback : &process;
        }
    }
    return fallback;
}

JsonValue ThreadJson(const ThreadView& view) {
    JsonValue thread = JsonValue::MakeObject();
    thread.Set("thread", view.thread);
    thread.Set("state", ThreadStateName(view.state));
    thread.Set("seconds_since_progress", view.since_progress);
    thread.Set("calls_per_second", Rate(view.calls_per_second));
    thread.Set("depth", view.slot->depth);
    if (!view.stack.IsEmpty()) {
        const StackFrameView& top = view.stack[view.stack.GetSize() - 1];
        thread.Set("function", top.name);
        thread.Set("seconds_in_function", top.age);
    } else {
        thread.Set("last_left", view.last_left);
        thread.Set("seconds_since_left", view.left_ago);
    }
    if (view.slot->waiting_on != 0) {
        thread.Set("waiting_on_lock", Format("{:#x}", view.slot->waiting_on));
        thread.Set("lock_held_by_thread", view.lock_holder != nullptr ? JsonValue(view.lock_holder->thread) : JsonValue());
    }
    if (view.slot->holding != 0) {
        thread.Set("holding_lock", Format("{:#x}", view.slot->holding));
    }
    JsonValue stack = JsonValue::MakeArray();
    for (const StackFrameView& frame : view.stack) {
        JsonValue entry = JsonValue::MakeObject();
        entry.Set("function", frame.name);
        entry.Set("seconds_open", frame.age);
        stack.Append(RValueCast(entry));
    }
    if (view.slot->depth > view.stack.GetSize()) {
        thread.Set("frames_not_stored", static_cast<u64>(view.slot->depth - view.stack.GetSize()));
    }
    thread.Set("stack", RValueCast(stack));
    return thread;
}

// What a table with no thread in it means, said where a caller would otherwise read "no threads".
JsonValue TracingNote(const ProcessSample& process) {
    if (!process.trace) {
        return JsonValue(process.trace_error.IsEmpty() ? String("no trace table") : process.trace_error);
    }
    if (process.trace->slots.IsEmpty() && process.source.alive) {
        return JsonValue("no thread has ever entered a traced function: the binary's scopes are not bound. QEMU's "
                         "device and executor record only when QEMU runs with AV_TRACE_SCOPES=1 (or AV_PERFETTO=1).");
    }
    return JsonValue();
}

} // namespace

McpTools::McpTools(Model& model) : m_model(model) {}

mcp::ServerInfo McpTools::Info() {
    mcp::ServerInfo info;
    info.name = "avinspector";
    info.version = "1.0.0";
    info.instructions =
        "Live state of an accel-virt stack on this host: the QEMU device's and executor's trace tables "
        "(threads, stacks, locks) and state dumps (av contexts, sessions, shares). Start with `stalls` when "
        "something hangs, `connections` when frames stop, `surfaces` for shared memory. Processes are named "
        "\"module/pid\" (e.g. \"device/1240628\"), a bare pid, or a module name for its live process. Rates are "
        "per second over the last 2 s of samples; take `sample_now` first when a fresh reading matters.";
    return info;
}

void McpTools::Register(mcp::Server& server) {
    server.RegisterTool("stalls",
                        "Every thread of every process that is stalled (inside a traced function with no "
                        "progress for stall_seconds) or blocked on a lock, with its whole stack and the thread "
                        "holding the lock. Empty when nothing is stuck.",
                        [this](const JsonValue& a) { return Stalls(a); },
                        Schema({{"stall_seconds", "number:seconds without progress that count as a stall (2)"}}));
    server.RegisterTool("processes",
                        "The accel-virt binaries found in /dev/shm (QEMU's device and executor), alive or gone, "
                        "with thread counts, crash records and whether their state dump answered.",
                        [this](const JsonValue& a) { return Processes(a); }, Schema({}));
    server.RegisterTool("threads",
                        "One process's threads from its trace table: state (stalled/waiting/running/idle), current "
                        "function and how long it has been in it, calls per second, locks, and each stack.",
                        [this](const JsonValue& a) { return Threads(a); },
                        Schema({{"process", "string:module/pid, pid or module (device, executor)"},
                                {"only_active", "boolean:leave idle threads out (false)"},
                                {"stall_seconds", "number:seconds without progress that count as a stall (2)"}}));
    server.RegisterTool("state",
                        "One process's state dump as `path key=value` lines. `filter` keeps lines whose path, key "
                        "or value contains it; `changed` keeps only fields that changed since the previous round "
                        "(with their old values).",
                        [this](const JsonValue& a) { return State(a); },
                        Schema({{"process", "string:module/pid, pid or module"},
                                {"filter", "string:substring of a path, key or value (case-insensitive)"},
                                {"changed", "boolean:only what changed since the last round (false)"}}));
    server.RegisterTool("connections",
                        "Every av context the device holds - one per guest driver context - with API, fence, "
                        "batches/s, ring bytes/s, pending work, worker and fault state; and the executor's sessions.",
                        [this](const JsonValue& a) { return Connections(a); }, Schema({}));
    server.RegisterTool("surfaces",
                        "Every GPU's share table in the executor: each shared object's origin, kind, size, whether "
                        "the guest still holds it, published/exported, holders, opens, writes and writes/s; and the "
                        "exportable image pool.",
                        [this](const JsonValue& a) { return Surfaces(a); }, Schema({}));
    server.RegisterTool("series",
                        "The history behind any number the dumps report (and each thread's call count). With "
                        "`match`, the keys containing it with their last value and rate; with `key`, its samples.",
                        [this](const JsonValue& a) { return SeriesTool(a); },
                        Schema({{"match", "string:substring of series keys to list"},
                                {"key", "string:one series key, as listed"},
                                {"samples", "number:how many of the newest samples to return (120)"}}));
    server.RegisterTool("sample_now", "Takes a sampling round now and waits for it (up to 3 s).",
                        [this](const JsonValue& a) { return SampleNow(a); }, Schema({}));
}

mcp::ToolResult McpTools::Stalls(const JsonValue& arguments) {
    const f64 stall_seconds = NumberArgument(arguments, "stall_seconds", 2.0);
    LockGuard<Mutex> hold(m_model.GetMutex());
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        return Err(String("no round sampled yet"));
    }
    JsonValue stuck = JsonValue::MakeArray();
    JsonValue untraced = JsonValue::MakeArray();
    for (const ProcessSample& process : current->processes) {
        if (!process.source.alive) {
            continue;
        }
        const JsonValue note = TracingNote(process);
        if (!note.IsNull()) {
            untraced.Append(process.source.Key());
        }
        for (const ThreadView& view : m_model.Threads(process, stall_seconds)) {
            if (view.state != ThreadState::Stalled && view.state != ThreadState::Waiting) {
                continue;
            }
            JsonValue thread = ThreadJson(view);
            thread.Set("process", process.source.Key());
            stuck.Append(RValueCast(thread));
        }
    }
    JsonValue result = JsonValue::MakeObject();
    result.Set("round", current->round);
    result.Set("stuck_threads", RValueCast(stuck));
    result.Set("processes_without_tracing", RValueCast(untraced));
    return result;
}

mcp::ToolResult McpTools::Processes(const JsonValue&) {
    LockGuard<Mutex> hold(m_model.GetMutex());
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        return Err(String("no round sampled yet"));
    }
    JsonValue list = JsonValue::MakeArray();
    for (const ProcessSample& process : current->processes) {
        JsonValue entry = JsonValue::MakeObject();
        entry.Set("process", process.source.Key());
        entry.Set("module", process.source.module);
        entry.Set("pid", process.source.pid);
        entry.Set("alive", process.source.alive);
        if (process.trace) {
            usize active = 0;
            for (const TraceSlot& slot : process.trace->slots) {
                active += slot.active ? 1 : 0;
            }
            entry.Set("threads_active", static_cast<u64>(active));
            entry.Set("threads_known", static_cast<u64>(process.trace->slots.GetSize()));
            if (process.trace->HasCrashed()) {
                JsonValue crash = JsonValue::MakeObject();
                crash.Set("code", Format("{:#x}", process.trace->crash_code));
                crash.Set("address", Format("{:#x}", process.trace->crash_address));
                crash.Set("thread", process.trace->crash_thread);
                entry.Set("crash", RValueCast(crash));
            }
        }
        const JsonValue note = TracingNote(process);
        if (!note.IsNull()) {
            entry.Set("tracing", note);
        }
        if (process.state) {
            entry.Set("dump_objects", static_cast<u64>(process.state->GetNodes().GetSize()));
            entry.Set("dump_seconds", process.state_seconds);
        } else if (!process.state_error.IsEmpty()) {
            entry.Set("dump_error", process.state_error);
        }
        list.Append(RValueCast(entry));
    }
    JsonValue result = JsonValue::MakeObject();
    result.Set("round", current->round);
    result.Set("seconds_since_sampled", m_model.SecondsSince(current->taken));
    result.Set("interval_ms", m_model.GetSampler().GetIntervalMs());
    result.Set("processes", RValueCast(list));
    return result;
}

mcp::ToolResult McpTools::Threads(const JsonValue& arguments) {
    const StringView wanted = StringArgument(arguments, "process");
    const bool only_active = BoolArgument(arguments, "only_active", false);
    const f64 stall_seconds = NumberArgument(arguments, "stall_seconds", 2.0);
    LockGuard<Mutex> hold(m_model.GetMutex());
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        return Err(String("no round sampled yet"));
    }
    const ProcessSample* process = ResolveProcess(*current, wanted.IsEmpty() ? StringView("device") : wanted);
    if (process == nullptr) {
        return Err(Format("no process '{}'; see `processes`", wanted));
    }
    JsonValue threads = JsonValue::MakeArray();
    for (const ThreadView& view : m_model.Threads(*process, stall_seconds)) {
        if (!only_active || view.state != ThreadState::Idle) {
            threads.Append(ThreadJson(view));
        }
    }
    JsonValue result = JsonValue::MakeObject();
    result.Set("process", process->source.Key());
    result.Set("alive", process->source.alive);
    const JsonValue note = TracingNote(*process);
    if (!note.IsNull()) {
        result.Set("tracing", note);
    }
    result.Set("threads", RValueCast(threads));
    return result;
}

mcp::ToolResult McpTools::State(const JsonValue& arguments) {
    const StringView wanted = StringArgument(arguments, "process");
    const StringView filter = StringArgument(arguments, "filter");
    const bool changed_only = BoolArgument(arguments, "changed", false);
    LockGuard<Mutex> hold(m_model.GetMutex());
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        return Err(String("no round sampled yet"));
    }
    const ProcessSample* process = ResolveProcess(*current, wanted.IsEmpty() ? StringView("device") : wanted);
    if (process == nullptr) {
        return Err(Format("no process '{}'; see `processes`", wanted));
    }
    if (!process->state) {
        return Err(Format("{} gave no dump: {}", process->source.Key(),
                          process->state_error.IsEmpty() ? StringView("it serves none") : process->state_error.GetView()));
    }
    const String key = process->source.Key();
    String text;
    usize lines = 0;
    bool truncated = false;
    for (const StateNode& node : process->state->GetNodes()) {
        String line;
        bool keep = filter.IsEmpty() || node.path.GetView().FindNoCase(filter) != StringView::Npos;
        for (const StateField& field : node.fields) {
            const String change = m_model.FieldChange(key, node.path, field.key, field.value);
            if (changed_only && change.IsEmpty()) {
                continue;
            }
            const bool matches = filter.IsEmpty() || keep || field.key.GetView().FindNoCase(filter) != StringView::Npos ||
                                 field.value.GetView().FindNoCase(filter) != StringView::Npos;
            if (!matches) {
                continue;
            }
            FormatTo(line, " {}={}", field.key, field.value);
            if (changed_only) {
                FormatTo(line, "(was {})", change);
            }
        }
        const bool any_field = !line.IsEmpty();
        if (changed_only ? !any_field : !(keep || any_field)) {
            continue;
        }
        if (lines == kMaxStateLines) {
            truncated = true;
            break;
        }
        FormatTo(text, "{}{}\n", node.path, line);
        ++lines;
    }
    JsonValue result = JsonValue::MakeObject();
    result.Set("process", key);
    result.Set("round", current->round);
    if (!process->state->GetError().IsEmpty()) {
        result.Set("binary_error", process->state->GetError());
    }
    result.Set("lines", static_cast<u64>(lines));
    result.Set("truncated", truncated);
    result.Set("dump", RValueCast(text));
    return result;
}

mcp::ToolResult McpTools::Connections(const JsonValue&) {
    LockGuard<Mutex> hold(m_model.GetMutex());
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        return Err(String("no round sampled yet"));
    }
    JsonValue contexts = JsonValue::MakeArray();
    for (const ContextView& view : m_model.Contexts()) {
        const StateNode& node = *view.node;
        JsonValue entry = JsonValue::MakeObject();
        entry.Set("process", view.process->source.Key());
        entry.Set("gpu", view.gpu);
        entry.Set("context", node.index);
        entry.Set("api", view.api);
        entry.Set("version", node.Value("version"));
        entry.Set("state", view.state);
        entry.Set("fence", node.Value("last_fence"));
        entry.Set("batches", node.Value("batches"));
        entry.Set("batches_per_second", Rate(view.batches_per_second));
        entry.Set("ring_bytes_per_second", Rate(view.ring_bytes_per_second));
        entry.Set("ring_order", node.Value("ring_order"));
        entry.Set("doorbell", node.Value("doorbell"));
        entry.Set("head", node.Value("head"));
        if (view.worker != nullptr) {
            entry.Set("worker_running", view.worker->Flag("running"));
            entry.Set("worker_pending", view.worker->Value("pending"));
            entry.Set("worker_stopping", view.worker->Flag("stopping"));
        }
        contexts.Append(RValueCast(entry));
    }
    JsonValue sessions = JsonValue::MakeArray();
    for (const ProcessSample& process : current->processes) {
        if (!process.state) {
            continue;
        }
        for (const StateNode* node : process.state->OfKind("session")) {
            JsonValue entry = JsonValue::MakeObject();
            entry.Set("process", process.source.Key());
            entry.Set("session", node->index);
            entry.Set("api", node->Value("api"));
            entry.Set("gpu", node->Value("gpu"));
            entry.Set("domain", node->Value("domain"));
            sessions.Append(RValueCast(entry));
        }
    }
    JsonValue result = JsonValue::MakeObject();
    result.Set("round", current->round);
    result.Set("contexts", RValueCast(contexts));
    result.Set("sessions", RValueCast(sessions));
    return result;
}

mcp::ToolResult McpTools::Surfaces(const JsonValue&) {
    LockGuard<Mutex> hold(m_model.GetMutex());
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        return Err(String("no round sampled yet"));
    }
    JsonValue gpus = JsonValue::MakeArray();
    for (const ProcessSample& process : current->processes) {
        if (!process.state) {
            continue;
        }
        const StateDump& dump = *process.state;
        const String key = process.source.Key();
        for (const StateNode* gpu : dump.OfKind("gpu_shares")) {
            JsonValue entry = JsonValue::MakeObject();
            entry.Set("process", key);
            entry.Set("gpu", gpu->index);
            JsonValue shares = JsonValue::MakeArray();
            for (const usize child : gpu->children) {
                const StateNode& node = dump.GetNodes()[child];
                if (node.kind == StringView("exportable_images")) {
                    JsonValue images = JsonValue::MakeObject();
                    for (const StateField& field : node.fields) {
                        images.Set(field.key, field.value);
                    }
                    entry.Set("exportable_images", RValueCast(images));
                    continue;
                }
                if (node.kind != StringView("shares")) {
                    continue;
                }
                JsonValue summary = JsonValue::MakeObject();
                for (const StateField& field : node.fields) {
                    summary.Set(field.key, field.value);
                }
                entry.Set("summary", RValueCast(summary));
                for (const usize share_index : node.children) {
                    const StateNode& share = dump.GetNodes()[share_index];
                    JsonValue item = JsonValue::MakeObject();
                    item.Set("share", share.index);
                    for (const StateField& field : share.fields) {
                        item.Set(field.key, field.value);
                    }
                    item.Set("writes_per_second", Rate(m_model.RateOf(key, share.path, "writes")));
                    shares.Append(RValueCast(item));
                }
            }
            entry.Set("shares", RValueCast(shares));
            gpus.Append(RValueCast(entry));
        }
    }
    JsonValue result = JsonValue::MakeObject();
    result.Set("round", current->round);
    result.Set("note", "origin 0 object, 1 D3D11, 2 D3D12; kind 0 a published Direct3D resource, 1 memory, "
                       "2 monitored fence, 3 native fence");
    result.Set("gpus", RValueCast(gpus));
    return result;
}

mcp::ToolResult McpTools::SeriesTool(const JsonValue& arguments) {
    const StringView match = StringArgument(arguments, "match");
    const StringView key = StringArgument(arguments, "key");
    const usize samples = static_cast<usize>(NumberArgument(arguments, "samples", 120.0));
    LockGuard<Mutex> hold(m_model.GetMutex());
    const History& history = m_model.GetHistory();
    JsonValue result = JsonValue::MakeObject();
    if (!key.IsEmpty()) {
        const Series* series = history.Find(key);
        if (series == nullptr) {
            return Err(Format("no series '{}'; list them with `match`", key));
        }
        JsonValue times = JsonValue::MakeArray();
        JsonValue values = JsonValue::MakeArray();
        const usize count = series->values.GetSize();
        for (usize i = count > samples ? count - samples : 0; i < count; ++i) {
            times.Append(series->times[i]);
            values.Append(series->values[i]);
        }
        result.Set("key", key);
        result.Set("rate_per_second", Rate(series->Rate(2.0)));
        result.Set("seconds", RValueCast(times));
        result.Set("values", RValueCast(values));
        return result;
    }
    JsonValue keys = JsonValue::MakeArray();
    usize listed = 0;
    history.ForEach([&](const String& name, const Series& series) {
        if (listed >= 500 || (!match.IsEmpty() && name.GetView().FindNoCase(match) == StringView::Npos)) {
            return;
        }
        JsonValue entry = JsonValue::MakeObject();
        entry.Set("key", name);
        entry.Set("last", series.Last() ? JsonValue(*series.Last()) : JsonValue());
        entry.Set("rate_per_second", Rate(series.Rate(2.0)));
        keys.Append(RValueCast(entry));
        ++listed;
    });
    result.Set("series", RValueCast(keys));
    result.Set("listed", static_cast<u64>(listed));
    return result;
}

mcp::ToolResult McpTools::SampleNow(const JsonValue&) {
    if (!m_model.SampleNow(3000)) {
        return Err(String("no round arrived within 3 s"));
    }
    LockGuard<Mutex> hold(m_model.GetMutex());
    JsonValue result = JsonValue::MakeObject();
    result.Set("round", m_model.GetCurrent()->round);
    result.Set("processes", static_cast<u64>(m_model.GetCurrent()->processes.GetSize()));
    return result;
}

} // namespace inspector
