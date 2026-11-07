#ifndef AV_D3D9_REFUSED_QUERIES_HPP
#define AV_D3D9_REFUSED_QUERIES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d9RefusedQueries {
public:
    static constexpr uint32_t most = 4096u;

    explicit D3d9RefusedQueries(BlockSource &blocks);
    D3d9RefusedQueries(const D3d9RefusedQueries &) = delete;
    D3d9RefusedQueries &operator=(const D3d9RefusedQueries &) = delete;

    [[nodiscard]] bool note(uint32_t query, uint32_t hresult);
    bool               answer(uint32_t query, uint32_t &hresult) const;
    bool               forget(uint32_t query);
    void               clear();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    KeyedSlots<uint32_t, uint32_t> refused_;
};

}

#endif
