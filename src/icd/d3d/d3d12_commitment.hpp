#ifndef AV_D3D_D3D12_COMMITMENT_HPP
#define AV_D3D_D3D12_COMMITMENT_HPP

#include <stdint.h>

namespace av::d3d {

class D3d12Commitment {
public:
    enum class Admission : uint32_t { refused, ask, defer };

    static constexpr uint64_t ask_from_bytes = uint64_t{256} << 20u;
    static constexpr uint64_t near_numerator = 9u;
    static constexpr uint64_t near_denominator = 10u;

    D3d12Commitment();
    D3d12Commitment(const D3d12Commitment &) = delete;
    D3d12Commitment &operator=(const D3d12Commitment &) = delete;

    void      learn(uint64_t budget_bytes);
    Admission charge(uint64_t bytes);
    void      release(uint64_t bytes);
    uint64_t  committed() const;
    uint64_t  budget() const;

    bool invariants_hold() const;

private:
    uint64_t budget_;
    uint64_t committed_;
};

}

#endif
