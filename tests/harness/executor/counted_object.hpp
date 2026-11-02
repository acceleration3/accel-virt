#ifndef AV_COUNTED_OBJECT_HPP
#define AV_COUNTED_OBJECT_HPP

#include "executor/shim/d3d_headers.hpp"

namespace av {

class CountedObject final : public IUnknown {
public:
    CountedObject();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override;
    ULONG STDMETHODCALLTYPE   AddRef() override;
    ULONG STDMETHODCALLTYPE   Release() override;

    ULONG references() const;

private:
    ULONG references_;
};

}

#endif
