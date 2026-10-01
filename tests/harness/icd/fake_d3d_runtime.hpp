/* d3d11.dll and D3D12Core.dll as avumd.dll sees them: the fake resolves the
 * export, checks the table the way the real runtime does, and calls the slots
 * in the real order - the role FakeOpengl32 plays for the OpenGL ICD.
 *
 * What it checks after an open is what D3D12Core checks, read from the binary
 * (research.md §12): the out adapter handle is non-zero, and slots two through
 * seven of the eight-slot table are non-null. The D3D11 side has five slots
 * and all five are required, since none of them is optional in a driver that
 * declares OpenAdapter10_2.
 *
 * A test drives the refusals through the `offer_*` and `ask_*` calls: a
 * runtime that hands in no callbacks, a version the driver does not serve, a
 * device block another component has scribbled on. */
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
}  // namespace av::d3d

namespace av {

class FakeD3dRuntime {
public:
    /* The word each runtime's driver names today; a test asking for anything
     * else is asking for a driver this is not. */
    static constexpr std::uint32_t d3d11_interface = (11u << 16) | 45u;
    static constexpr std::uint32_t d3d11_runtime_build = 26100u;
    /* What d3d9.dll offers first: Interface 9 at the newest version it knows
     * (docs/research/d3d9/reverse-engineering.md §1.3). */
    static constexpr std::uint32_t d3d9_interface = 9u;
    static constexpr std::uint32_t d3d9_newest_version = 0x11000u;
    static constexpr std::size_t   device_block_bytes = 64u;
    static constexpr std::size_t   table_bytes = 2048u;

    FakeD3dRuntime(d3d::UmdRoot &root, d3d::RuntimeKind runtime);
    FakeD3dRuntime(const FakeD3dRuntime &) = delete;
    FakeD3dRuntime &operator=(const FakeD3dRuntime &) = delete;

    /* Refusals a test sets before `open`. */
    void offer_no_callbacks();
    void offer_no_table();
    void ask_for_interface(std::uint32_t interface_version);
    /* Direct3D 9 only: the Version OpenAdapter is called with. */
    void ask_for_version(std::uint32_t version);
    /* Refusals a test sets before `create_device`. */
    void offer_no_dxgi_callbacks();
    void offer_no_dxgi_table();
    void scribble_the_device_block();
    /* Before `open`: the driver's LUID query is answered as avgpu.sys answers
     * it for the adapter a runtime opened, naming `luid`, where otherwise it is
     * refused. */
    void answer_luid(std::uint64_t luid);

    /* Calls the export and, on success, checks the table as the real runtime
     * does. A driver that answered hr_ok with a hole in its table is a
     * failure, not a pass. */
    d3d::Hresult open();
    bool         table_is_whole() const;
    void        *adapter() const;

    /* The slots, each through the pointer the table came back with. */
    d3d::Hresult supported_versions(std::uint32_t *entries, std::uint64_t *words);
    d3d::Hresult caps(std::uint32_t type, void *data, std::uint32_t bytes);
    std::size_t  calc_private_device_size();
    d3d::Hresult create_device();
    d3d::Hresult close_adapter();
    /* D3D12 only; a call on a D3D11 fake is the test's mistake and answers
     * hr_invalid_arg without touching the driver. */
    d3d::Hresult optional_ddi_tables(std::uint32_t *entries, d3d::D3d12ddiTableRequestRecord *requests);
    /* Fills `bytes` of a zeroed table, 2 KiB unless said. */
    d3d::Hresult fill_ddi_table(std::uint32_t table_type, std::size_t bytes = table_bytes);
    /* Slots the last fill left non-null. */
    std::uint32_t filled_slots() const;
    void         destroy_device();
    /* Direct3D 9: the DriverVersion OpenAdapter answered, and the device
     * handle the last create handed back. */
    std::uint32_t driver_version() const;
    void         *device9() const;
    d3d::Hresult  destroy_device9();

    /* What the last create was given, so a test can drive the DXGI slots. */
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
    /* What the adapter handle points at once a LUID is answered: the query
     * is given that handle and nothing else. */
    std::uint64_t                        luid_;
    int                                  runtime_core_layer_;
    int                                  kt_callbacks_;
    int                                  um_callbacks_;
};

}  // namespace av

#endif  // AV_FAKE_D3D_RUNTIME_HPP
