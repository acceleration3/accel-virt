#ifndef AV_ADAPTER_SEARCH_HPP
#define AV_ADAPTER_SEARCH_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;

class AdapterSearch {
public:
    explicit AdapterSearch(Kmt &kmt);
    ~AdapterSearch();
    AdapterSearch(const AdapterSearch &) = delete;
    AdapterSearch &operator=(const AdapterSearch &) = delete;

    [[nodiscard]] Status find_avgpu(uint64_t luid);
    [[nodiscard]] Status close();
    bool                 found() const;
    uint32_t             adapter() const;

private:
    Kmt     &kmt_;
    uint32_t adapter_;
};

}

#endif
