#ifndef AV_EXECUTOR_D3D12_REFUSED_CREATES_HPP
#define AV_EXECUTOR_D3D12_REFUSED_CREATES_HPP

#include <stdint.h>

namespace av {

class D3d12RefusedCreates {
public:
    static constexpr uint32_t most = 64u;

    D3d12RefusedCreates();
    D3d12RefusedCreates(const D3d12RefusedCreates &) = delete;
    D3d12RefusedCreates &operator=(const D3d12RefusedCreates &) = delete;

    [[nodiscard]] bool note(uint32_t id, uint32_t hresult);
    bool               forget(uint32_t id);
    bool               holds(uint32_t id) const;
    uint32_t           hresult_of(uint32_t id) const;
    uint32_t           count() const;
    void               clear();

    bool invariants_hold() const;

private:
    struct Refusal {
        uint32_t id_;
        uint32_t hresult_;
    };

    uint32_t find(uint32_t id) const;

    Refusal  rows_[most];
    uint32_t count_;
};

}

#endif
