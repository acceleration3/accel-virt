/* A host device several sessions share, which HostDeviceRenewal declares lost
 * and has made again: what only the device's own runtime can say or do.
 * Virtual because there are two real implementations - the GPU's Direct3D 11
 * device (DXVK) and its Direct3D 12 device (vkd3d-proton) - and a scripted
 * one makes the renewal testable without a GPU. */
#ifndef AV_RENEWABLE_DEVICE_HPP
#define AV_RENEWABLE_DEVICE_HPP

namespace av {

class RenewableDevice {
public:
    RenewableDevice(const RenewableDevice &) = delete;
    RenewableDevice &operator=(const RenewableDevice &) = delete;

    /* Whether a device is made at all. */
    virtual bool present() const = 0;
    /* Whether the runtime says the device was removed; false with none. */
    virtual bool removed() = 0;
    /* With no tenant left on it: what anything still holds of the old device
     * let go of, the old device with it, and the device made anew. Whether
     * it was. */
    [[nodiscard]] virtual bool remake() = 0;

protected:
    RenewableDevice() = default;
    ~RenewableDevice() = default;
};

}  // namespace av

#endif  // AV_RENEWABLE_DEVICE_HPP
