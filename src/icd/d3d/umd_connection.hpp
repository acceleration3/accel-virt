#ifndef AV_D3D_UMD_CONNECTION_HPP
#define AV_D3D_UMD_CONNECTION_HPP

#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}

namespace av::d3d {

struct UmdConnectionProbe;

class UmdConnection {
public:
    explicit UmdConnection(Kmt &kmt);
    ~UmdConnection();
    UmdConnection(const UmdConnection &) = delete;
    UmdConnection &operator=(const UmdConnection &) = delete;

    [[nodiscard]] Status acquire(uint64_t luid);
    [[nodiscard]] Status release();

    bool     open() const;
    bool     holds(uint64_t luid) const;
    bool     shares_adapter(const UmdConnection &other) const;
    uint32_t adapter() const;
    uint32_t opens() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    AdapterSearch search_;
    uint64_t      luid_;
    uint32_t      opens_;

    friend struct UmdConnectionProbe;
};

}

#endif
