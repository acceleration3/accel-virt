#ifndef AV_SCRIPTED_ADAPTER_ORDER_HPP
#define AV_SCRIPTED_ADAPTER_ORDER_HPP

#include "icd/opengl/adapter_order.hpp"

#include <cstdint>

namespace av {

class ScriptedAdapterOrder final : public glicd::AdapterOrder {
public:
    ScriptedAdapterOrder() : first_(0u), drawn_(0u), asked_(0u) {}

    void set_first(std::uint64_t luid) { first_ = luid; }
    void set_drawn(std::uint64_t luid) { drawn_ = luid; }
    std::uint32_t asked() const { return asked_; }

    std::uint64_t listed_first() override { asked_++; return first_; }
    std::uint64_t drawn_on(void *) override { asked_++; return drawn_; }

private:
    std::uint64_t first_;
    std::uint64_t drawn_;
    std::uint32_t asked_;
};

}

#endif
