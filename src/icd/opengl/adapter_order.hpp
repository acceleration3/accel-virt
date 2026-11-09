#ifndef AV_GLICD_ADAPTER_ORDER_HPP
#define AV_GLICD_ADAPTER_ORDER_HPP

#include <stdint.h>

namespace av::glicd {

class AdapterOrder {
public:
    AdapterOrder(const AdapterOrder &) = delete;
    AdapterOrder &operator=(const AdapterOrder &) = delete;

    virtual uint64_t listed_first() = 0;
    virtual uint64_t drawn_on(void *dc) = 0;

protected:
    AdapterOrder() = default;
    ~AdapterOrder() = default;
};

}

#endif
