#ifndef AV_RENEWABLE_DEVICE_HPP
#define AV_RENEWABLE_DEVICE_HPP

namespace av {

class RenewableDevice {
public:
    RenewableDevice(const RenewableDevice &) = delete;
    RenewableDevice &operator=(const RenewableDevice &) = delete;

    virtual bool present() const = 0;
    virtual bool removed() = 0;
    [[nodiscard]] virtual bool remake() = 0;

protected:
    RenewableDevice() = default;
    ~RenewableDevice() = default;
};

}

#endif
