#ifndef AV_UM_SLICE_OFFER_HPP
#define AV_UM_SLICE_OFFER_HPP

#include <stdint.h>

namespace av {
class ClientTransport;
class DriverLog;
}

namespace av::d3d {

class SliceOffer {
public:
    SliceOffer(const SliceOffer &) = delete;
    SliceOffer &operator=(const SliceOffer &) = delete;

    virtual bool granted(ClientTransport &host, DriverLog &lines) const = 0;

    uint64_t alignment() const;

protected:
    explicit SliceOffer(uint64_t alignment);
    ~SliceOffer() = default;

private:
    uint64_t alignment_;
};

}

#endif
