#ifndef AV_D3D11_BATCH_PROGRESS_HPP
#define AV_D3D11_BATCH_PROGRESS_HPP

#include <stdint.h>

struct ID3D11Fence;

namespace av {

class EventfdWake;

class D3d11BatchProgress {
public:
    explicit D3d11BatchProgress(EventfdWake &wake);
    ~D3d11BatchProgress();
    D3d11BatchProgress(const D3d11BatchProgress &) = delete;
    D3d11BatchProgress &operator=(const D3d11BatchProgress &) = delete;

    void         adopt(ID3D11Fence *fence);
    ID3D11Fence *fence() const;
    uint64_t     next() const;
    void         signalled();
    void         release();

    uint64_t marked() const;
    uint64_t reached() const;
    bool     pending() const;
    bool     polled() const;

    bool invariants_hold() const;

private:
    EventfdWake &wake_;
    ID3D11Fence *fence_;
    uint64_t     marked_;
    uint64_t     released_at_;
    bool         watched_;
};

}

#endif
