/* A Direct3D 12 object that keeps private data as vkd3d-proton's do, for a
 * test of what the device keeps on its objects without a GPU: one blob under
 * one GUID, answered with the size it holds, DXGI_ERROR_MORE_DATA to a
 * buffer too small for it and DXGI_ERROR_NOT_FOUND under any other GUID.
 * A root signature and a command signature add no methods to
 * ID3D12DeviceChild, so this is either. */
#ifndef AV_PRIVATE_DATA_OBJECT_HPP
#define AV_PRIVATE_DATA_OBJECT_HPP

#include "executor/shim/d3d_headers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace av {

template <typename Interface>
class PrivateDataObject final : public Interface {
public:
    static constexpr std::size_t capacity = 1024u;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE Release() override { return 1u; }
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
        if (refuse_ || size > capacity) return E_OUTOFMEMORY;
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

    /* An object that will keep nothing, as one out of memory would. */
    void refuse() { refuse_ = true; }
    /* Whatever it holds, `size` bytes of it: what another writer under the
     * same GUID would have left. */
    void resize(std::size_t size) { size_ = size; }

private:
    GUID          guid_ = {};
    std::size_t   size_ = 0u;
    std::uint8_t  bytes_[capacity] = {};
    bool          refuse_ = false;
};

}  // namespace av

#endif  // AV_PRIVATE_DATA_OBJECT_HPP
