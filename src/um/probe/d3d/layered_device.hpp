#ifndef AV_LAYERED_DEVICE_HPP
#define AV_LAYERED_DEVICE_HPP

#include <unknwn.h>

namespace av {

class LayeredDevice {
public:
    explicit LayeredDevice(IUnknown *device);

    bool layered() const;

private:
    IUnknown *device_;
};

}

#endif
