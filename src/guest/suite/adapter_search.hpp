/* Finds the adapter the suite tests among every adapter D3DKMT opens: the
 * first whose PCI identity matches is kept, every other handle is closed at
 * once. The kept adapter is closed by close() or the destructor. */
#ifndef AV_ADAPTER_SEARCH_HPP
#define AV_ADAPTER_SEARCH_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;
class PciIdentity;

class AdapterSearch {
public:
    static constexpr uint32_t max_adapters = 16u;

    explicit AdapterSearch(Kmt &kmt);
    ~AdapterSearch();
    AdapterSearch(const AdapterSearch &) = delete;
    AdapterSearch &operator=(const AdapterSearch &) = delete;

    /* invalid_state once found; the enumeration's failure; not_found when no
     * adapter matches. */
    [[nodiscard]] Status find(const PciIdentity &identity);
    /* Closes the kept adapter, if any. */
    [[nodiscard]] Status close();
    bool                 found() const;
    uint32_t             adapter() const;

private:
    void keep_or_close(uint32_t adapter, const PciIdentity &identity);

    Kmt     &kmt_;
    uint32_t adapter_;
};

}  // namespace av

#endif  // AV_ADAPTER_SEARCH_HPP
