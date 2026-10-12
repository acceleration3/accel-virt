#include "data/Sampler.h"

#include "data/Clock.h"
#include "data/StateClient.h"
#include "threading/LockGuard.h"
#include "time/Duration.h"

namespace inspector {
namespace {

// A dump that takes longer is reported as missing for the round; the next round asks again.
constexpr int kStateTimeoutMs = 1500;

} // namespace

const ProcessSample* Sample::Find(StringView key) const noexcept {
    for (const ProcessSample& process : processes) {
        if (process.source.Key().GetView() == key) {
            return &process;
        }
    }
    return nullptr;
}

Sampler::Sampler(String directory) : m_directory(RValueCast(directory)) {}

Sampler::~Sampler() {
    Stop();
}

void Sampler::Start() {
    if (m_thread.IsJoinable()) {
        return;
    }
    m_stop.Store(false);
    m_thread = Thread([this] { Run(); });
}

void Sampler::Stop() {
    if (!m_thread.IsJoinable()) {
        return;
    }
    m_stop.Store(true);
    m_wake.Set();
    m_thread.Join();
}

void Sampler::SetIntervalMs(int interval_ms) noexcept {
    m_interval_ms.Store(interval_ms < 50 ? 50 : interval_ms);
    m_wake.Set();
}

void Sampler::SetPaused(bool paused) noexcept {
    m_paused.Store(paused);
    m_wake.Set();
}

void Sampler::RefreshNow() noexcept {
    m_forced.Store(true);
    m_wake.Set();
}

Optional<Sample> Sampler::TakeLatest() {
    LockGuard<Mutex> hold(m_mutex);
    Optional<Sample> taken = RValueCast(m_latest);
    m_latest = NullOpt;
    return taken;
}

void Sampler::Run() {
    u64 round = 0;
    while (!m_stop.Load()) {
        if (!m_paused.Load() || m_forced.Exchange(false)) {
            Sample sample = Collect(++round);
            LockGuard<Mutex> hold(m_mutex);
            m_latest = RValueCast(sample);
        }
        (void)m_wake.WaitFor(Milliseconds<>(m_interval_ms.Load()));
    }
}

Sample Sampler::Collect(u64 round) const {
    Sample sample;
    sample.taken = NowTicks();
    sample.round = round;
    for (Source& source : ScanSources(m_directory)) {
        ProcessSample& process = sample.processes.EmplaceBack();
        if (!source.trace_path.IsEmpty()) {
            if (Optional<DynamicArray<u8>> bytes = ReadWholeFile(source.trace_path, process.trace_error)) {
                process.trace = ParseTraceTable(bytes->AsSpan(), process.trace_error);
            }
        }
        if (source.alive && !source.state_path.IsEmpty()) {
            const u64 asked = NowTicks();
            if (Optional<String> text = FetchState(source.state_path, kStateTimeoutMs, process.state_error)) {
                process.state = StateDump::Parse(*text);
            }
            process.state_seconds = TicksToSeconds(NowTicks() - asked);
        }
        process.source = RValueCast(source);
    }
    return sample;
}

} // namespace inspector
