#include "harness/framework/test_registry.hpp"
#include "harness/icd/fake_d3d_runtime.hpp"
#include "harness/icd/recording_host_driver.hpp"
#include "harness/icd/umd_rig.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_funcs_record.hpp"
#include "icd/d3d/ddi/d3dddiarg_open_adapter_record.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/umd_host_driver.hpp"

#include <cstdint>

using av::FakeD3dRuntime;
using av::Status;
using av::RecordingHostDriver;
using av::UmdRig;
using av::d3d::RuntimeKind;
using namespace av::d3d;

namespace {

constexpr std::uint32_t win7_interface = 0x2003u;
constexpr std::uint32_t vista_interface = 0x000cu;
constexpr std::uint32_t caps_d3d9 = 13u;

}

TEST(umd_root_open_adapter9, opens_at_the_newest_version_and_answers_windows_7s)
{
    UmdRig         rig;
    FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d9);
    CHECK_EQ(runtime.open(), hr_ok);
    CHECK(runtime.table_is_whole());
    CHECK_EQ(runtime.driver_version(), win7_interface);
    CHECK(rig.root().invariants_hold());
    CHECK_EQ(runtime.close_adapter(), hr_ok);
    CHECK_EQ(rig.kmt().open_objects(), 0u);
}

TEST(umd_root_open_adapter9, refuses_what_is_not_interface_9_at_windows_7_or_later)
{
    UmdRig         rig;
    FakeD3dRuntime old_runtime(rig.root(), RuntimeKind::d3d9);
    old_runtime.ask_for_version(vista_interface);
    CHECK_EQ(old_runtime.open(), hr_no_interface);
    FakeD3dRuntime exact(rig.root(), RuntimeKind::d3d9);
    exact.ask_for_version(win7_interface);
    CHECK_EQ(exact.open(), hr_ok);
    CHECK_EQ(exact.close_adapter(), hr_ok);
    FakeD3dRuntime eight(rig.root(), RuntimeKind::d3d9);
    eight.ask_for_interface(8u);
    CHECK_EQ(eight.open(), hr_no_interface);
    CHECK_EQ(rig.root().open_adapter9(nullptr), hr_invalid_arg);
    FakeD3dRuntime no_table(rig.root(), RuntimeKind::d3d9);
    no_table.offer_no_table();
    CHECK_EQ(no_table.open(), hr_invalid_arg);
    CHECK_EQ(rig.kmt().open_objects(), 0u);
    CHECK(rig.root().invariants_hold());
}

TEST(umd_root_open_adapter9, a_refused_open_fills_none_of_the_records)
{
    constexpr std::uint32_t untouched = 0x1234u;
    UmdRig                     rig;
    D3dddiAdapterFuncsRecord   table = {};
    D3dddiargOpenAdapterRecord args = {};
    args.Interface = 9u;
    args.Version = win7_interface;
    args.pAdapterFuncs = &table;
    args.DriverVersion = untouched;
    CHECK_EQ(rig.root().open_adapter9(&args), hr_invalid_arg);
    CHECK(table.pfnGetCaps == nullptr);
    CHECK(table.pfnCreateDevice == nullptr);
    CHECK(table.pfnCloseAdapter == nullptr);
    CHECK(args.hAdapter == nullptr);
    CHECK_EQ(args.DriverVersion, untouched);
    CHECK_EQ(rig.kmt().open_objects(), 0u);
}

TEST(umd_root_create_device9, a_device_is_the_host_drivers_and_holds_its_adapter)
{
    RecordingHostDriver driver;
    UmdRig              rig(&driver);
    FakeD3dRuntime      runtime(rig.root(), RuntimeKind::d3d9);
    CHECK_EQ(runtime.open(), hr_ok);
    CHECK_EQ(runtime.create_device(), hr_ok);
    CHECK_EQ(driver.builds9(), 1u);
    CHECK(runtime.device9() != nullptr);
    CHECK(runtime.device9() == driver.device9());
    CHECK(rig.root().invariants_hold());
    CHECK_EQ(runtime.close_adapter(), hr_fail);
    CHECK_EQ(runtime.destroy_device9(), hr_ok);
    CHECK_EQ(driver.tear_downs(), 1u);
    CHECK(driver.torn_runtime() == RuntimeKind::d3d9);
    CHECK_EQ(rig.root().destroy_device9(driver.device9()), hr_invalid_arg);
    CHECK_EQ(runtime.close_adapter(), hr_ok);
    CHECK(rig.root().invariants_hold());
}

TEST(umd_root_create_device9, build9_is_asked_on_the_opened_adapter_and_tear_down9_knows_its_own)
{
    RecordingHostDriver driver;
    UmdRig              rig(&driver);
    FakeD3dRuntime      runtime(rig.root(), RuntimeKind::d3d9);
    CHECK_EQ(runtime.open(), hr_ok);
    CHECK_EQ(runtime.create_device(), hr_ok);
    std::uint64_t luid = 0u;
    CHECK_STATUS(rig.kmt().adapter_luid(driver.kmt_adapter(), luid), Status::ok);
    CHECK_EQ(luid, UmdRig::adapter_luid);
    UmdHostDriver &host = driver;
    CHECK(host.tear_down9(&luid) == nullptr);
    CHECK_EQ(driver.tear_downs(), 0u);
    CHECK_EQ(runtime.destroy_device9(), hr_ok);
    CHECK_EQ(driver.tear_downs(), 1u);
    CHECK_EQ(runtime.close_adapter(), hr_ok);
}

TEST(umd_root_create_device9, a_refused_create_leaves_nothing_made)
{
    RecordingHostDriver driver;
    UmdRig              rig(&driver);
    FakeD3dRuntime      runtime(rig.root(), RuntimeKind::d3d9);
    CHECK_EQ(runtime.open(), hr_ok);
    driver.answer(hr_out_of_memory, hr_ok, hr_ok);
    CHECK_EQ(runtime.create_device(), hr_out_of_memory);
    CHECK(runtime.device9() == nullptr);
    CHECK_EQ(runtime.close_adapter(), hr_ok);
    CHECK_EQ(rig.root().create_device9(nullptr, nullptr), hr_invalid_arg);
    CHECK(rig.root().invariants_hold());
}

TEST(umd_root_create_device9, with_no_host_driver_nothing_is_made)
{
    UmdRig         rig;
    FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d9);
    CHECK_EQ(runtime.open(), hr_ok);
    CHECK_EQ(runtime.create_device(), hr_not_impl);
    CHECK_EQ(rig.root().destroy_device9(nullptr), hr_invalid_arg);
    CHECK_EQ(runtime.close_adapter(), hr_ok);
}

TEST(umd_root_create_device9, refuses_an_adapter_another_runtime_opened)
{
    RecordingHostDriver driver;
    UmdRig              rig(&driver);
    FakeD3dRuntime      eleven(rig.root(), RuntimeKind::d3d10_11);
    CHECK_EQ(eleven.open(), hr_ok);
    void *create[16] = {};
    CHECK_EQ(rig.root().create_device9(eleven.adapter(), create), hr_invalid_arg);
    CHECK_EQ(driver.builds9(), 0u);
    CHECK_EQ(eleven.close_adapter(), hr_ok);
}

TEST(umd_root_open_adapter9, caps_reach_the_host_driver_as_direct3d_9s)
{
    RecordingHostDriver driver;
    UmdRig              rig(&driver);
    FakeD3dRuntime      runtime(rig.root(), RuntimeKind::d3d9);
    CHECK_EQ(runtime.open(), hr_ok);
    std::uint8_t caps[0x130] = {};
    CHECK_EQ(runtime.caps(caps_d3d9, caps, sizeof caps), hr_ok);
    CHECK(driver.caps_runtime() == RuntimeKind::d3d9);
    CHECK_EQ(driver.caps_type(), caps_d3d9);
    CHECK_EQ(runtime.close_adapter(), hr_ok);
}
