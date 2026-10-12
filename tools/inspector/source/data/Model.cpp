#include "data/Model.h"

#include "data/Clock.h"
#include "text/Format.h"
#include "threading/LockGuard.h"
#include "threading/ThisThread.h"
#include "time/Duration.h"

namespace inspector {
namespace {

// A frame's age against the table's clock: zero for a stamp the reader's clock is behind.
f64 AgeOf(u64 entered, u64 now) {
    return now > entered ? TicksToSeconds(now - entered) : 0.0;
}

String ProgressKey(StringView process, u32 thread) {
    return Format("{} {}", process, thread);
}

// SubApi (src/proto/context/sub_api.hpp).
StringView SubApiName(StringView value) {
    if (value == "1") return "Vulkan";
    if (value == "2") return "OpenGL";
    if (value == "4") return "OpenCL";
    if (value == "8") return "Direct3D";
    return value;
}

} // namespace

StringView ThreadStateName(ThreadState state) {
    switch (state) {
    case ThreadState::Stalled: return "stalled";
    case ThreadState::Waiting: return "waiting";
    case ThreadState::Running: return "running";
    case ThreadState::Idle: return "idle";
    }
    return "?";
}

Model::Model(ModelOptions options) : m_options(RValueCast(options)), m_sampler(m_options.shm_directory) {}

Model::~Model() {
    Stop();
}

StringView Model::GetSourceRoot() const noexcept {
    return m_options.source_root.IsEmpty() ? ScopeNames::DefaultRoot() : m_options.source_root.GetView();
}

void Model::Start() {
    m_started = NowTicks();
    m_names.LoadTree(GetSourceRoot());
    m_sampler.SetIntervalMs(m_options.interval_ms);
    m_sampler.Start();
}

void Model::Stop() {
    m_sampler.Stop();
}

f64 Model::Seconds(u64 ticks) const noexcept {
    return ticks > m_started ? TicksToSeconds(ticks - m_started) : 0.0;
}

f64 Model::SecondsSince(u64 ticks) const noexcept {
    const u64 now = NowTicks();
    return now > ticks ? TicksToSeconds(now - ticks) : 0.0;
}

bool Model::Update() {
    Optional<Sample> latest = m_sampler.TakeLatest();
    if (!latest) {
        return false;
    }
    LockGuard<Mutex> hold(m_mutex);
    const f64 time = Seconds(latest->taken);
    for (const ProcessSample& process : latest->processes) {
        const String key = process.source.Key();
        if (process.state) {
            m_history.Record(key, *process.state, time);
        }
        if (!process.trace) {
            continue;
        }
        for (const TraceSlot& slot : process.trace->slots) {
            Progress& progress = m_progress.GetOrAdd(ProgressKey(key, slot.thread));
            if (progress.entries != slot.entries || progress.changed_at == 0.0) {
                progress.entries = slot.entries;
                progress.changed_at = time;
            }
            m_history.Record(Format("{} thread/{} entries", key, slot.thread), static_cast<f64>(slot.entries), time);
        }
    }
    // The round being replaced is what "changed" is measured against.
    if (m_current) {
        for (ProcessSample& process : m_current->processes) {
            if (process.state) {
                m_previous_states.InsertOrAssign(process.source.Key(), RValueCast(*process.state));
            }
        }
    }
    m_current = RValueCast(latest);
    return true;
}

bool Model::SampleNow(int timeout_ms) {
    u64 before = 0;
    {
        LockGuard<Mutex> hold(m_mutex);
        before = m_current ? m_current->round : 0;
    }
    m_sampler.RefreshNow();
    const u64 deadline = NowTicks() + static_cast<u64>(timeout_ms) * (kTicksPerSecond / 1000);
    while (NowTicks() < deadline) {
        Update();
        {
            LockGuard<Mutex> hold(m_mutex);
            if (m_current && m_current->round > before) {
                return true;
            }
        }
        ThisThread::SleepFor(Milliseconds<>(10));
    }
    return false;
}

const ProcessSample* Model::Find(StringView process_key) const noexcept {
    return m_current ? m_current->Find(process_key) : nullptr;
}

Optional<f64> Model::RateOf(StringView process, StringView path, StringView field, f64 window) const {
    const Series* series = m_history.Find(History::SeriesKey(process, path, field));
    return series != nullptr ? series->Rate(window) : NullOpt;
}

String Model::FieldChange(StringView process, StringView path, StringView key, StringView value) const {
    const StateDump* previous = m_previous_states.Find(process);
    if (previous == nullptr) {
        return {};
    }
    const StateNode* node = previous->Find(path);
    const StateField* field = node != nullptr ? node->Field(key) : nullptr;
    if (field == nullptr) {
        return String("new");
    }
    return field->value.GetView() == value ? String() : field->value;
}

bool Model::IsNewNode(StringView process, StringView path) const {
    const StateDump* previous = m_previous_states.Find(process);
    return previous != nullptr && previous->Find(path) == nullptr;
}

DynamicArray<ThreadView> Model::Threads(const ProcessSample& process, f64 stall_seconds) const {
    DynamicArray<ThreadView> views;
    if (!process.trace || !m_current) {
        return views;
    }
    const TraceTable& table = *process.trace;
    const String key = process.source.Key();
    const u64 now = m_current->taken;
    const f64 time = Seconds(now);
    for (const TraceSlot& slot : table.slots) {
        ThreadView view;
        view.slot = &slot;
        view.thread = slot.thread;
        const Progress* progress = m_progress.Find(ProgressKey(key, slot.thread));
        view.since_progress = progress != nullptr ? time - progress->changed_at : 0.0;
        if (const Series* calls = m_history.Find(Format("{} thread/{} entries", key, slot.thread))) {
            view.calls_per_second = calls->Rate(2.0);
        }
        for (const TraceFrame& frame : slot.frames) {
            view.stack.PushBack(StackFrameView{m_names.Find(frame.name_id), AgeOf(frame.entered, now)});
        }
        const f64 top_age = view.stack.IsEmpty() ? 0.0 : view.stack[view.stack.GetSize() - 1].age;
        if (!slot.active) {
            view.state = ThreadState::Idle;
            view.last_left = slot.last_left != 0 ? m_names.Find(slot.last_left) : String();
            view.left_ago = slot.left_at != 0 ? AgeOf(slot.left_at, now) : 0.0;
        } else if (slot.waiting_on != 0) {
            view.state = ThreadState::Waiting;
            for (const TraceSlot& other : table.slots) {
                if (&other != &slot && other.holding == slot.waiting_on) {
                    view.lock_holder = &other;
                }
            }
        } else if (view.since_progress >= stall_seconds && top_age >= stall_seconds) {
            view.state = ThreadState::Stalled;
        } else {
            view.state = ThreadState::Running;
        }
        views.PushBack(RValueCast(view));
    }
    // Worst first, then by thread: a stall is what anyone looking at threads is looking for.
    for (usize i = 1; i < views.GetSize(); ++i) {
        for (usize j = i; j > 0; --j) {
            const ThreadView& a = views[j - 1];
            const ThreadView& b = views[j];
            if (a.state < b.state || (a.state == b.state && a.thread <= b.thread)) {
                break;
            }
            Swap(views[j - 1], views[j]);
        }
    }
    return views;
}

DynamicArray<ContextView> Model::Contexts() const {
    DynamicArray<ContextView> views;
    if (!m_current) {
        return views;
    }
    for (const ProcessSample& process : m_current->processes) {
        if (!process.state) {
            continue;
        }
        const StateDump& dump = *process.state;
        const String key = process.source.Key();
        for (const StateNode* node : dump.OfKind("context")) {
            if (dump.Ancestor(*node, "contexts") == nullptr) {
                continue;
            }
            ContextView view;
            view.process = &process;
            view.node = node;
            const StateNode* device = dump.Ancestor(*node, "device");
            view.gpu = device != nullptr ? device->Value("gpu") : StringView("?");
            view.api = SubApiName(node->Value("sub_api"));
            view.worker = dump.Find(Format("{}/worker/0", node->path));
            view.batches_per_second = RateOf(key, node->path, "batches");
            view.ring_bytes_per_second = RateOf(key, node->path, "head");
            if (node->Flag("faulted")) {
                view.state = "FAULTED";
            } else if (!node->Flag("bound") || !node->Flag("attached")) {
                view.state = "not bound";
            } else if (view.worker != nullptr && !view.worker->Flag("running")) {
                view.state = "worker stopped";
            } else {
                view.state = "live";
            }
            views.PushBack(RValueCast(view));
        }
    }
    return views;
}

} // namespace inspector
