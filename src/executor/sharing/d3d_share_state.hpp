#ifndef AV_D3D_SHARE_STATE_HPP
#define AV_D3D_SHARE_STATE_HPP

#include "executor/sharing/share_origin.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class D3dRetired;

class D3dShareState {
public:
    explicit D3dShareState(ShareOrigin origin);
    D3dShareState(const D3dShareState &) = delete;
    D3dShareState &operator=(const D3dShareState &) = delete;

    IUnknown *resource() const;
    IUnknown *view() const;
    IUnknown *shown() const;
    IUnknown *referenced(IUnknown *object) const;

    void      keep_resource(IUnknown *resource);
    void      keep_view(IUnknown *view, D3dRetired &retired);
    void      keep_shown(IUnknown *copy, D3dRetired &retired);
    IUnknown *open(bool &live_of_presented);
    void      expect_shown();

    void      retire_all(D3dRetired &retired);
    void      retire_direct3d11(D3dRetired &retired);
    void      retire_direct3d12(D3dRetired &retired);

    void      written();
    uint64_t  writes() const;
    uint64_t  opens() const;

private:
    void      retire(IUnknown *&held, D3dRetired &retired);

    IUnknown   *resource_;
    IUnknown   *view_;
    IUnknown   *shown_;
    uint64_t    writes_;
    uint64_t    opens_;
    ShareOrigin origin_;
    bool        presented_;
    bool        told_;
};

}

#endif
