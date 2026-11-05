#ifndef AV_D3D9_BATCH_PROGRESS_HPP
#define AV_D3D9_BATCH_PROGRESS_HPP

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DQuery9;

namespace av {

class D3d9BatchProgress {
public:
    static constexpr uint32_t in_flight = 32u;

    D3d9BatchProgress();
    ~D3d9BatchProgress();
    D3d9BatchProgress(const D3d9BatchProgress &) = delete;
    D3d9BatchProgress &operator=(const D3d9BatchProgress &) = delete;

    [[nodiscard]] bool signalled(IDirect3DDevice9 *device);
    void               forget(IDirect3DDevice9 *device);
    void               release();

    uint64_t next() const;
    uint64_t marked() const;
    uint64_t reached() const;
    bool     pending() const;

    bool invariants_hold() const;

private:
    struct Slot {
        IDirect3DQuery9  *query_;
        IDirect3DDevice9 *device_;
        uint64_t          mark_;
    };

    void               reap();
    bool               done(const Slot &slot) const;
    Slot              &at(uint32_t index);
    const Slot        &at(uint32_t index) const;
    [[nodiscard]] bool prepare(Slot &slot, IDirect3DDevice9 *device);
    void               flush(IDirect3DDevice9 *device) const;

    Slot     slots_[in_flight];
    uint32_t first_;
    uint32_t used_;
    uint64_t marked_;
    uint64_t reaped_;
};

}

#endif
