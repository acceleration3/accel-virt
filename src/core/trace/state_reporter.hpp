/* The root of a binary's state dump: the driver's adapters, an ICD's
 * connections and handles, the device's contexts (docs/trace.md). A report
 * never waits for a lock; one it cannot take at once is written as busy. */
#ifndef AV_STATE_REPORTER_HPP
#define AV_STATE_REPORTER_HPP

namespace av {

class StateWriter;

class StateReporter {
public:
    StateReporter() = default;
    StateReporter(const StateReporter &) = delete;
    StateReporter &operator=(const StateReporter &) = delete;

    virtual void report(StateWriter &writer) = 0;

protected:
    ~StateReporter() = default;
};

}  // namespace av

#endif  // AV_STATE_REPORTER_HPP
