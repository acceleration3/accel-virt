/* The tests' D3dAdapterSource: one DXGI adapter and one Direct3D 10 device the
 * test names, each with the LUID it says; every other object is on no adapter.
 * It counts the questions, so a test can tell a lookup refused before it asked
 * from one that asked. */
#ifndef AV_SCRIPTED_D3D_ADAPTER_SOURCE_HPP
#define AV_SCRIPTED_D3D_ADAPTER_SOURCE_HPP

#include "icd/opencl/d3d_adapter_source.hpp"

#include <cstdint>

namespace av {

class ScriptedD3dAdapterSource final : public clicd::D3dAdapterSource {
public:
    ScriptedD3dAdapterSource();

    /* `adapter` is an IDXGIAdapter of LUID `luid` from now on. */
    void          name_adapter(void *adapter, std::uint64_t luid);
    /* `device` is an ID3D10Device on an adapter of LUID `luid` from now on. */
    void          name_device(void *device, std::uint64_t luid);
    std::uint64_t adapter_luid(void *dxgi_adapter) override;
    std::uint64_t device_luid(void *d3d10_device) override;
    std::uint32_t asked() const;

private:
    void         *adapter_;
    std::uint64_t adapter_luid_;
    void         *device_;
    std::uint64_t device_luid_;
    std::uint32_t asked_;
};

}  // namespace av

#endif  // AV_SCRIPTED_D3D_ADAPTER_SOURCE_HPP
