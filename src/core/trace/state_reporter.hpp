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

}

#endif
