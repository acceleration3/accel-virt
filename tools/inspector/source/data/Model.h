#ifndef DATA_MODEL_H
#define DATA_MODEL_H

#include "containers/DynamicArray.h"
#include "containers/HashMap.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Types.h"
#include "data/History.h"
#include "data/Sampler.h"
#include "data/ScopeNames.h"
#include "threading/Mutex.h"

namespace inspector {

struct ModelOptions {
    String shm_directory = "/dev/shm";
    String source_root;
    int interval_ms = 250;
};

enum class ThreadState : u8 { Stalled, Waiting, Running, Idle };
StringView ThreadStateName(ThreadState state);

struct StackFrameView {
    String name;
    f64 age = 0.0;
};

struct ThreadView {
    const TraceSlot* slot = nullptr;
    u32 thread = 0;
    ThreadState state = ThreadState::Idle;
    f64 since_progress = 0.0;
    Optional<f64> calls_per_second;
    const TraceSlot* lock_holder = nullptr;
    DynamicArray<StackFrameView> stack;
    String last_left;
    f64 left_ago = 0.0;
};

struct ContextView {
    const ProcessSample* process = nullptr;
    const StateNode* node = nullptr;
    const StateNode* worker = nullptr;
    StringView gpu;
    StringView api;
    Optional<f64> batches_per_second;
    Optional<f64> ring_bytes_per_second;
    StringView state;
};

class Model {
public:
    explicit Model(ModelOptions options);
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;

    void Start();
    void Stop();
    bool Update();
    bool SampleNow(int timeout_ms);

    Mutex& GetMutex() noexcept { return m_mutex; }
    Sampler& GetSampler() noexcept { return m_sampler; }
    const ScopeNames& GetNames() const noexcept { return m_names; }
    StringView GetSourceRoot() const noexcept;
    const History& GetHistory() const noexcept { return m_history; }
    History& GetHistory() noexcept { return m_history; }
    const Optional<Sample>& GetCurrent() const noexcept { return m_current; }

    f64 Seconds(u64 ticks) const noexcept;
    f64 SecondsSince(u64 ticks) const noexcept;

    const ProcessSample* Find(StringView process_key) const noexcept;
    Optional<f64> RateOf(StringView process, StringView path, StringView field, f64 window = 2.0) const;
    String FieldChange(StringView process, StringView path, StringView key, StringView value) const;
    bool IsNewNode(StringView process, StringView path) const;

    DynamicArray<ThreadView> Threads(const ProcessSample& process, f64 stall_seconds) const;
    DynamicArray<ContextView> Contexts() const;

private:
    ModelOptions m_options;
    Sampler m_sampler;
    ScopeNames m_names;
    mutable Mutex m_mutex;
    History m_history;
    Optional<Sample> m_current;
    HashMap<String, StateDump> m_previous_states;
    struct Progress {
        u64 entries = 0;
        f64 changed_at = 0.0;
    };
    HashMap<String, Progress> m_progress;
    u64 m_started = 0;
};

}

#endif
