/* The byte the guest wrote to the virtio status register, and the fence
 * interrupts that follow it. A driver that stops clears DRIVER_OK without
 * resetting the device - Windows does this whenever it restarts a display
 * driver - so the clear, not only a reset, must release them: a connection
 * that outlives the driver which asked for it is registered a second time by
 * the next driver's MSI-X toggle, which KVM refuses and QEMU asserts on. */
#ifndef AV_DRIVER_STATUS_HPP
#define AV_DRIVER_STATUS_HPP

#include <stdint.h>

namespace av {

class FenceInterrupts;
class Host;

class DriverStatus {
public:
    /* VIRTIO_CONFIG_S_DRIVER_OK. */
    static constexpr uint8_t driver_ok = 4u;

    DriverStatus(Host &host, FenceInterrupts &interrupts);
    DriverStatus(const DriverStatus &) = delete;
    DriverStatus &operator=(const DriverStatus &) = delete;

    /* Main loop: the byte the guest wrote. DRIVER_OK connects the fence
     * interrupts and anything without it releases them; a repeat asks the host
     * nothing. A refusal is logged with the status that caused it and leaves
     * them off. */
    void    set(uint8_t status);
    /* Main loop: a virtio reset, which QEMU delivers whether or not the guest
     * wrote 0 first. */
    void    reset();
    uint8_t value() const;

    bool invariants_hold() const;

private:
    Host            &host_;
    FenceInterrupts &interrupts_;
    uint8_t          value_;
};

}  // namespace av

#endif  // AV_DRIVER_STATUS_HPP
