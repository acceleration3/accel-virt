#ifndef AV_FAKE_D3D_RUNTIME_HPP
#define AV_FAKE_D3D_RUNTIME_HPP

#include "icd/d3d/ddi/d3d10_2ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3d12ddi_table_request_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_callbacks_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/dxgi1_6_1_ddi_base_functions_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_base_callbacks_record.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/runtime_kind.hpp"

#include <cstddef>
#include <cstdint>

namespace av::d3d {
class UmdRoot;
}

namespace av {

class FakeD3dRuntime {
public:
    static constexpr std::uint32_t d3d11_interface = (11u << 16) | 45u;
    static constexpr std::uint32_t d3d11_runtime_build = 26100u;
    static constexpr std::uint32_t d3d9_interface = 9u;
    static constexpr std::uint32_t d3d9_newest_version = 0x11000u;
    static constexpr std::size_t   device_block_bytes = 64u;
    static constexpr std::size_t   table_bytes = 2048u;

    FakeD3dRuntime(d3d::UmdRoot &root, d3d::RuntimeKind runtime);
    FakeD3dRuntime(const FakeD3dRuntime &) = delete;
    FakeD3dRuntime &operator=(const FakeD3dRuntime &) = delete;

    void offer_no_callbacks();
    void offer_no_table();
    void ask_for_interface(std::uint32_t interface_version);
    void ask_for_version(std::uint32_t version);
    void offer_no_dxgi_callbacks();
    void offer_no_dxgi_table();
    void scribble_the_device_block();
    void answer_luid(std::uint64_t luid);
    void offer_no_adapter_query();
    void answer_unreadable_adapter_info();

    d3d::Hresult open();
    bool         table_is_whole() const;
    void        *adapter() const;

    d3d::Hresult supported_versions(std::uint32_t *entries, std::uint64_t *words);
    d3d::Hresult caps(std::uint32_t type, void *data, std::uint32_t bytes);
    std::size_t  calc_private_device_size();
    d3d::Hresult create_device();
    d3d::Hresult close_adapter();
    d3d::Hresult optional_ddi_tables(std::uint32_t *entries, d3d::D3d12ddiTableRequestRecord *requests);
    d3d::Hresult fill_ddi_table(std::uint32_t table_type, std::size_t bytes = table_bytes);
    std::uint32_t filled_slots() const;
    void         destroy_device();
    std::uint32_t driver_version() const;
    void         *device9() const;
    d3d::Hresult  destroy_device9();

    void                                   *device_block();
    d3d::Dxgi161DdiBaseFunctionsRecord     &dxgi_table();
    const d3d::DxgiDdiBaseCallbacksRecord  &dxgi_callbacks() const;
private:
    d3d::UmdRoot                        &root_;
    d3d::RuntimeKind                     runtime_;
    d3d::D3dddiAdapterCallbacksRecord    callbacks_;
    d3d::D3dddiAdapterFuncsRecord        funcs9_;
    std::uint32_t                        version9_;
    std::uint32_t                        driver_version_;
    void                                *device9_;
    d3d::D3d102ddiAdapterFuncsRecord     funcs10_;
    d3d::D3d12ddiAdapterFuncsRecord      funcs12_;
    d3d::DxgiDdiBaseCallbacksRecord      dxgi_callbacks_;
    d3d::Dxgi161DdiBaseFunctionsRecord   dxgi_table_;
    void                                *adapter_;
    std::uint32_t                        interface_;
    bool                                 no_callbacks_;
    bool                                 no_table_;
    bool                                 no_dxgi_callbacks_;
    bool                                 no_dxgi_table_;
    bool                                 scribble_;
    alignas(16) unsigned char            block_[device_block_bytes];
    void                                *table_[table_bytes / sizeof(void *)];
    int                                  runtime_device_;
    std::uint64_t                        luid_;
    int                                  runtime_core_layer_;
    int                                  kt_callbacks_;
    int                                  um_callbacks_;
};

}

#endif
