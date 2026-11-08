#ifndef AV_CONTEXT_GATE_HPP
#define AV_CONTEXT_GATE_HPP

namespace av {

class ContextGate {
public:
    ContextGate(const ContextGate &) = delete;
    ContextGate &operator=(const ContextGate &) = delete;

    virtual void enter() = 0;
    virtual void leave() = 0;
    virtual bool inside() const = 0;

protected:
    ContextGate() = default;
    ~ContextGate() = default;
};

}

#endif
