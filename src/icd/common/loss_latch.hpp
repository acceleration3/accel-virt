/* The first fault cause one driver context was lost to - a Direct3D device,
 * an OpenGL or OpenCL context, a Vulkan instance - and the entry points that
 * found it lost, each said once in the root's KeyedLog.
 *
 * The cause is the fence page's fault code (HostChannel::fault_code), or
 * none for a loss the host gave no reason for: a send the kernel refused, a
 * sibling context's fault that lost the device they share. The first cause
 * stands, because a loss cascades and what follows is a consequence of it;
 * an API answers with it (D3D's DEVICE_RESET or DEVICE_REMOVED, OpenGL's
 * GUILTY or UNKNOWN context reset), each in its own one-line spelling.
 *
 * Any thread: the cause is one word, set once. */
#ifndef AV_ICD_LOSS_LATCH_HPP
#define AV_ICD_LOSS_LATCH_HPP

#include "proto/submission/fault_code.hpp"

#include <stdint.h>

namespace av {
class KeyedLog;
class StateWriter;
}  // namespace av

namespace av::icd {

struct LossLatchProbe;

class LossLatch {
public:
    explicit LossLatch(KeyedLog &lines);
    LossLatch(const LossLatch &) = delete;
    LossLatch &operator=(const LossLatch &) = delete;

    /* The context is lost to `cause`, as the host wrote it - a code no fault
     * has is kept, and is not guilty; a context lost already keeps its first. */
    void      lose(FaultCode cause);
    bool      lost() const;
    /* The first cause; none while live. */
    FaultCode cause() const;
    /* Whether the context's own commands lost it - the device refused its
     * ring, a batch or a command it could not decode - rather than the host
     * device it ran on or the host's executor. */
    bool      guilty() const;
    /* Whether lost: logged the first time `entry` (a literal) finds it so. */
    [[nodiscard]] bool refuses(const char *entry);
    /* A context made again starts live. */
    void      forget();

    /* Its fields on the line `writer` has open. */
    void report(StateWriter &writer) const;
    /* Live is a word of 0, and any other word carries the lost bit: a cause
     * of none is still a loss. */
    bool invariants_hold() const;

private:
    static constexpr uint32_t lost_bit = 0x80000000u;

    uint32_t word() const;

    KeyedLog &lines_;
    /* 0 while live; lost_bit with the cause beside it once lost. */
    uint32_t  word_;

    friend struct LossLatchProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_LOSS_LATCH_HPP
