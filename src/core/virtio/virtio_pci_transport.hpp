/* Drives one virtio-pci device (virtio 1.2 4.1): reset, feature negotiation,
 * MSI-X vectors, queue activation and DRIVER_OK. Every value read back from the
 * device is checked; any failure after ACKNOWLEDGE leaves the device FAILED. */
#ifndef AV_VIRTIO_PCI_TRANSPORT_HPP
#define AV_VIRTIO_PCI_TRANSPORT_HPP

#include "core/platform/platform.hpp"
#include "core/platform/register_window.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class VirtQueue;
struct VirtioPciTransportProbe;

class VirtioPciTransport {
public:
    static constexpr uint64_t feature_version_1 = uint64_t{1} << 32u;

    VirtioPciTransport(Platform &platform, RegisterWindow &common, RegisterWindow &notify,
                       RegisterWindow &isr, RegisterWindow &device, uint32_t notify_multiplier);
    VirtioPciTransport(const VirtioPciTransport &) = delete;
    VirtioPciTransport &operator=(const VirtioPciTransport &) = delete;

    /* 3.1.1 steps 1-6. The negotiated set is offered & (wanted | required). */
    [[nodiscard]] Status negotiate(uint64_t wanted, uint64_t required);
    /* 3.1.1 step 8, after queues are activated. */
    [[nodiscard]] Status start();
    /* Writes status 0 and waits for the device to acknowledge; the device is
     * unconfigured either way. */
    [[nodiscard]] Status reset();
    /* Writes status 0 and returns: for DxgkDdiResetDevice, at any IRQL. */
    void                 abandon();
    [[nodiscard]] Status assign_config_vector(uint16_t vector);
    /* Sizes, allocates and enables `queue` on the device, bound to MSI-X `vector`. */
    [[nodiscard]] Status activate_queue(VirtQueue &queue, uint16_t vector);
    void                 notify(const VirtQueue &queue);
    void                 fail();

    uint64_t features() const;
    uint16_t queue_count();
    bool     needs_reset();
    uint8_t  read_isr();
    [[nodiscard]] Status read_device_u32(uint64_t offset, uint32_t &value);

    bool invariants_hold() const;

private:
    enum class Stage : uint8_t { unconfigured, negotiated, running, failed };

    static constexpr uint32_t reset_polls        = 1000u;
    static constexpr uint32_t reset_poll_us      = 1000u;
    static constexpr uint16_t no_vector          = 0xFFFFu;

    void                 add_status(uint8_t bits);
    uint64_t             read_device_features();
    void                 write_driver_features(uint64_t features);

    Platform       &platform_;
    RegisterWindow &common_;
    RegisterWindow &notify_;
    RegisterWindow &isr_;
    RegisterWindow &device_;
    uint32_t        notify_multiplier_;
    uint64_t        features_;
    Stage           stage_;

    friend struct VirtioPciTransportProbe;
};

}  // namespace av

#endif  // AV_VIRTIO_PCI_TRANSPORT_HPP
