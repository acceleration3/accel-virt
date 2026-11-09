#ifndef AV_UM_DXGI_ADAPTER_ORDER_HPP
#define AV_UM_DXGI_ADAPTER_ORDER_HPP

#include "icd/opengl/adapter_order.hpp"

namespace av {

class DxgiAdapterOrder final : public glicd::AdapterOrder {
public:
    constexpr DxgiAdapterOrder() = default;

    uint64_t listed_first() override;
    uint64_t drawn_on(void *dc) override;
};

}

#endif
