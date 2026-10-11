/* A COM object that only counts its references, for a test of what holds a
 * Direct3D resource and when it lets go, without a GPU. It is never freed by
 * its count: the test owns it, and reads the count. */
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

    /* The test's own reference counts as one. */
    ULONG references() const;

private:
    ULONG references_;
};

}  // namespace av

#endif  // AV_COUNTED_OBJECT_HPP
