#include "harness/executor/counted_object.hpp"

namespace av {

CountedObject::CountedObject() : references_(1u) {}

HRESULT STDMETHODCALLTYPE CountedObject::QueryInterface(REFIID, void **object)
{
    *object = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CountedObject::AddRef() { return ++references_; }

ULONG STDMETHODCALLTYPE CountedObject::Release() { return --references_; }

ULONG CountedObject::references() const { return references_; }

}  // namespace av
