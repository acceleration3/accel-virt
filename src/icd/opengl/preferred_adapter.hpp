#ifndef AV_GLICD_PREFERRED_ADAPTER_HPP
#define AV_GLICD_PREFERRED_ADAPTER_HPP

#include <stdint.h>

namespace av {
class Kmt;
}

namespace av::glicd {

class AdapterOrder;

class PreferredAdapter {
public:
    PreferredAdapter(Kmt &kmt, AdapterOrder &order);
    PreferredAdapter(const PreferredAdapter &) = delete;
    PreferredAdapter &operator=(const PreferredAdapter &) = delete;

    uint64_t choose(void *dc);
    uint64_t default_luid();

private:
    bool avgpu_has(uint64_t luid);

    Kmt          &kmt_;
    AdapterOrder &order_;
};

}

#endif
