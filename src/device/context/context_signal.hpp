#ifndef AV_CONTEXT_SIGNAL_HPP
#define AV_CONTEXT_SIGNAL_HPP

#include <stdint.h>

namespace av {

class FenceReporter;
class Host;
class Worker;
struct ContextSignalProbe;

class ContextSignal {
public:
    ContextSignal(Host &host, Worker &worker, FenceReporter &fences);
    ContextSignal(const ContextSignal &) = delete;
    ContextSignal &operator=(const ContextSignal &) = delete;

    void raise();
    void drain();

    bool     pending() const;
    uint64_t raised() const;
    uint64_t delivered() const;

    bool invariants_hold() const;

private:
    Host          &host_;
    Worker        &worker_;
    FenceReporter &fences_;
    bool           pending_;
    uint64_t       raised_;
    uint64_t       delivered_;

    friend struct ContextSignalProbe;
};

}

#endif
