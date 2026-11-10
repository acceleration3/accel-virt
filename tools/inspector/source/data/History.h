#ifndef DATA_HISTORY_H
#define DATA_HISTORY_H

#include "containers/DynamicArray.h"
#include "containers/HashMap.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Types.h"

namespace inspector {

class StateDump;

struct Series {
    DynamicArray<f64> times;
    DynamicArray<f64> values;

    Optional<f64> Rate(f64 window) const noexcept;
    Optional<f64> Last() const noexcept;
};

class History {
public:
    static constexpr usize kCapacity = 2400;
    static constexpr f64 kWindowSeconds = 600.0;

    static String SeriesKey(StringView process, StringView path, StringView field);

    void Record(StringView process, const StateDump& dump, f64 time);
    void Record(StringView key, f64 value, f64 time);

    const Series* Find(StringView key) const noexcept;
    template <typename Fn>
    void ForEach(Fn&& fn) const {
        m_series.ForEach(fn);
    }
    usize GetSize() const noexcept { return m_series.GetSize(); }
    usize Prune(f64 now);
    void Clear() noexcept { m_series.Clear(); }

private:
    HashMap<String, Series> m_series;
};

}

#endif
