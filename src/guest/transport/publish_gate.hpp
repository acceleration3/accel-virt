/* What a held transport's next batch waits for before it reaches the host.
 *
 * A runtime orders a device's work against other processes with waits it
 * queues on the device's kernel context - after a flip-model present, a wait
 * for DWM to give back the buffer drawn next - and on Windows whatever the
 * device submits after that waits too. Batches go to the host on the
 * transport's ring, which dxgkrnl never sees, so a driver that presents holds
 * the transport (KmtTransport::hold) and the next batch written passes this
 * first. Without it Affinity's next frame cleared the buffer DWM was still
 * composing, and its canvas showed black on half its frames while it scrolled.
 *
 * Two implementations: the user-mode drivers', which waits for the kernel
 * context, and the tests', which counts. */
#ifndef AV_GUEST_PUBLISH_GATE_HPP
#define AV_GUEST_PUBLISH_GATE_HPP

namespace av {

class PublishGate {
public:
    PublishGate(const PublishGate &) = delete;
    PublishGate &operator=(const PublishGate &) = delete;

    /* Returns once what the batch must wait for is done. A wait that cannot
     * be made is the gate's to report: the batch goes regardless, as it did
     * before there was a gate, since holding it would stop the device. */
    virtual void pass() = 0;

protected:
    PublishGate() = default;
    ~PublishGate() = default;
};

}  // namespace av

#endif  // AV_GUEST_PUBLISH_GATE_HPP
