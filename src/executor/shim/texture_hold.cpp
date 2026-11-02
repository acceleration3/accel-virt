#include "executor/shim/texture_hold.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/exportable_images.hpp"

namespace av {

TextureHold::TextureHold(ExportableImages &owner) : owner_(&owner), references_(0u), held_(false)
{
    AV_SCOPE("TextureHold::TextureHold");
}

int32_t TextureHold::query_interface(const void *iid, void **object)
{
    AV_SCOPE("TextureHold::query_interface");
    if (object == nullptr) return E_POINTER;
    *object = nullptr;
    if (iid == nullptr || !(*static_cast<const GUID *>(iid) == __uuidof(IUnknown))) return E_NOINTERFACE;
    *object = this;
    add_ref();
    return S_OK;
}

uint32_t TextureHold::add_ref()
{
    AV_SCOPE("TextureHold::add_ref");
    __atomic_store_n(&held_, true, __ATOMIC_RELEASE);
    return __atomic_add_fetch(&references_, 1u, __ATOMIC_ACQ_REL);
}

uint32_t TextureHold::release()
{
    AV_SCOPE("TextureHold::release");
    uint32_t references = __atomic_load_n(&references_, __ATOMIC_ACQUIRE);
    while (references != 0u && !__atomic_compare_exchange_n(&references_, &references, references - 1u, false,
                                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    }
    if (references == 1u) owner_->retired();
    return references == 0u ? 0u : references - 1u;
}

bool TextureHold::retired() const
{
    AV_SCOPE("TextureHold::retired");
    return __atomic_load_n(&held_, __ATOMIC_ACQUIRE) && __atomic_load_n(&references_, __ATOMIC_ACQUIRE) == 0u;
}

}
