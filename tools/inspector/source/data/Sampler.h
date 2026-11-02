#ifndef DATA_SAMPLER_H
#define DATA_SAMPLER_H

#include "containers/DynamicArray.h"
#include "containers/String.h"
#include "core/Atomic.h"
#include "core/Optional.h"
#include "core/Types.h"
#include "data/Sources.h"
#include "data/StateDump.h"
#include "data/TraceTable.h"
#include "threading/Event.h"
#include "threading/Mutex.h"
#include "threading/Thread.h"

namespace inspector {

struct ProcessSample {
    Source source;
    Optional<TraceTable> trace;
    String trace_error;
    Optional<StateDump> state;
    String state_error;
    f64 state_seconds = 0.0;
};

struct Sample {
    u64 taken = 0;
    u64 round = 0;
    DynamicArray<ProcessSample> processes;

    const ProcessSample* Find(StringView key) const noexcept;
};

class Sampler {
public:
    explicit Sampler(String directory = "/dev/shm");
    ~Sampler();
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&&) = delete;
    Sampler& operator=(Sampler&&) = delete;

    void Start();
    void Stop();

    void SetIntervalMs(int interval_ms) noexcept;
    int GetIntervalMs() const noexcept { return m_interval_ms.Load(); }
    void SetPaused(bool paused) noexcept;
    bool IsPaused() const noexcept { return m_paused.Load(); }
    void RefreshNow() noexcept;

    Optional<Sample> TakeLatest();

private:
    void Run();
    Sample Collect(u64 round) const;

    String m_directory;
    Thread m_thread;
    Event m_wake{EventReset::Auto};
    Atomic<bool> m_stop{false};
    Atomic<bool> m_paused{false};
    Atomic<bool> m_forced{false};
    Atomic<int> m_interval_ms{250};
    Mutex m_mutex;
    Optional<Sample> m_latest;
};

}

#endif
