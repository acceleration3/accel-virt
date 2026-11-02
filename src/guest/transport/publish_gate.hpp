#ifndef AV_GUEST_PUBLISH_GATE_HPP
#define AV_GUEST_PUBLISH_GATE_HPP

namespace av {

class PublishGate {
public:
    PublishGate(const PublishGate &) = delete;
    PublishGate &operator=(const PublishGate &) = delete;

    virtual void pass() = 0;

protected:
    PublishGate() = default;
    ~PublishGate() = default;
};

}

#endif
