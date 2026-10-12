#include "data/History.h"

#include "data/StateDump.h"
#include "text/Format.h"

namespace inspector {

Optional<f64> Series::Rate(f64 window) const noexcept {
    const usize count = values.GetSize();
    if (count < 2) {
        return NullOpt;
    }
    const f64 newest = times[count - 1];
    usize first = count - 1;
    while (first > 0 && newest - times[first - 1] <= window) {
        --first;
    }
    if (first == count - 1) {
        first = count - 2;
    }
    const f64 span = newest - times[first];
    if (span <= 0.0) {
        return NullOpt;
    }
    return (values[count - 1] - values[first]) / span;
}

Optional<f64> Series::Last() const noexcept {
    if (values.IsEmpty()) {
        return NullOpt;
    }
    return values[values.GetSize() - 1];
}

String History::SeriesKey(StringView process, StringView path, StringView field) {
    return Format("{} {} {}", process, path, field);
}

void History::Record(StringView key, f64 value, f64 time) {
    Series& series = m_series.GetOrAdd(String(key));
    series.times.PushBack(time);
    series.values.PushBack(value);
    if (series.values.GetSize() > kCapacity) {
        const usize drop = series.values.GetSize() - kCapacity;
        series.times.RemoveRange(0, drop);
        series.values.RemoveRange(0, drop);
    }
}

void History::Record(StringView process, const StateDump& dump, f64 time) {
    for (const StateNode& node : dump.GetNodes()) {
        for (const StateField& field : node.fields) {
            if (const Optional<i64> number = node.Integer(field.key)) {
                Record(SeriesKey(process, node.path, field.key), static_cast<f64>(*number), time);
            }
        }
    }
}

const Series* History::Find(StringView key) const noexcept {
    return m_series.Find(key);
}

} // namespace inspector
