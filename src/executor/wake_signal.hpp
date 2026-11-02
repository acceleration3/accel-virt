#ifndef AV_WAKE_SIGNAL_HPP
#define AV_WAKE_SIGNAL_HPP

namespace av {

class WakeSignal {
public:
    WakeSignal();
    WakeSignal(const WakeSignal &) = delete;
    WakeSignal &operator=(const WakeSignal &) = delete;

    void bind(void (*signal)(void *context), void *context);
    void clear();
    bool bound() const;
    bool raise() const;

private:
    void (*signal_)(void *context);
    void *context_;
};

}

#endif
