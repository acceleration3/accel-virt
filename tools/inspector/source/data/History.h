#ifndef DATA_HISTORY_H
#define DATA_HISTORY_H

// Every numeric field of every dump, over time: what a rate is computed from and a plot draws.
// A series is named by its process, its object's path and its field
// ("device/1240628 module/0/device/0/contexts/0/context/66 last_fence") and keeps the newest
// kCapacity samples.

#include "containers/DynamicArray.h"
#include "containers/HashMap.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Types.h"

namespace inspector {

class StateDump;

struct Series {
    DynamicArray<f64> times;  // seconds
    DynamicArray<f64> values;

    // Change per second over the newest `window` seconds; null with fewer than two samples in it.
    Optional<f64> Rate(f64 window) const noexcept;
    Optional<f64> Last() const noexcept;
};

class History {
public:
    static constexpr usize kCapacity = 2400;

    static String SeriesKey(StringView process, StringView path, StringView field);

    // Every field of `dump` that reads as an integer, at `time` seconds.
    void Record(StringView process, const StateDump& dump, f64 time);
    void Record(StringView key, f64 value, f64 time);

    const Series* Find(StringView key) const noexcept;
    // `fn(const String& key, const Series&)` for every series, in no order.
    template <typename Fn>
    void ForEach(Fn&& fn) const {
        m_series.ForEach(fn);
    }
    usize GetSize() const noexcept { return m_series.GetSize(); }
    void Clear() noexcept { m_series.Clear(); }

private:
    HashMap<String, Series> m_series;
};

} // namespace inspector

#endif
