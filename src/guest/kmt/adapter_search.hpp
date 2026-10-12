/* One avgpu adapter, held open: the one a Direct3D runtime opened, or the one
 * an ICD that shows a single device is on. Which adapters there are, and in
 * what order, is AvgpuAdapters'. */
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

    /* The avgpu adapter whose LUID is `luid`; for a `luid` of 0, the first
     * AvgpuAdapters lists - the one the primary display is on. invalid_state
     * once found; the enumeration's failure; not_found when no adapter
     * matches. */
    [[nodiscard]] Status find_avgpu(uint64_t luid);
    /* Closes the kept adapter, if any. */
    [[nodiscard]] Status close();
    bool                 found() const;
    uint32_t             adapter() const;

private:
    Kmt     &kmt_;
    uint32_t adapter_;
};

}  // namespace av

#endif  // AV_ADAPTER_SEARCH_HPP
