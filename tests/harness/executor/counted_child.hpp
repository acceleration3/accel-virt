#ifndef AV_COUNTED_CHILD_HPP
#define AV_COUNTED_CHILD_HPP

#include "executor/shim/d3d_headers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace av {

template <typename Interface>
class CountedChild : public Interface {
public:
    static constexpr std::size_t capacity = 256u;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { return --references_; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *size, void *data) override
    {
        if (size_ == 0u || std::memcmp(&guid, &guid_, sizeof guid_) != 0) return DXGI_ERROR_NOT_FOUND;
        const UINT room = *size;
        *size = static_cast<UINT>(size_);
        if (data == nullptr) return S_OK;
        if (room < size_) return DXGI_ERROR_MORE_DATA;
        std::memcpy(data, bytes_, size_);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT size, const void *data) override
    {
        if (size > capacity) return E_OUTOFMEMORY;
        guid_ = guid;
        size_ = size;
        std::memcpy(bytes_, data, size);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void **device) override
    {
        *device = nullptr;
        return E_NOTIMPL;
    }

    ULONG references() const { return references_; }

private:
    ULONG        references_ = 1u;
    GUID         guid_ = {};
    std::size_t  size_ = 0u;
    std::uint8_t bytes_[capacity] = {};
};

}

#endif
