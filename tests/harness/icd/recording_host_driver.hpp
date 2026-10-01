/* The root's host driver in a test: it makes nothing on any host, answers
 * each call with what the test told it to, and remembers what it was asked -
 * which runtime's device it was handed, with which kernel adapter, which
 * table, which device torn down - so a test judges the root's dispatch. */
#ifndef AV_RECORDING_HOST_DRIVER_HPP
#define AV_RECORDING_HOST_DRIVER_HPP

#include "icd/d3d/hresult.hpp"
#include "icd/d3d/runtime_kind.hpp"
#include "icd/d3d/umd_host_driver.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class RecordingHostDriver final : public d3d::UmdHostDriver {
public:
    RecordingHostDriver();

    d3d::Hresult build(d3d::UmdDevice &adopted, d3d::D3d10ddiargCreateDeviceRecord &create,
                       std::uint32_t kmt_adapter) override;
    d3d::Hresult build12(d3d::UmdDevice &adopted, d3d::D3d12ddiargCreateDeviceRecord &create,
                         std::uint32_t kmt_adapter) override;
    d3d::Hresult caps(d3d::UmdAdapter &adapter, d3d::D3d102ddiargGetCapsRecord &args,
                      std::uint32_t kmt_adapter) override;
    d3d::Hresult fill_table12(std::uint32_t table_type, void *table, std::size_t bytes, std::uint32_t index,
                              void *runtime_table) override;
    void         tear_down(d3d::UmdDevice &device, void *block) override;
    /* A Direct3D 9 device made is the address of a counter here, handed back
     * through hDevice; tearing it down names the adapter it was made on. */
    d3d::Hresult     build9(d3d::UmdAdapter &adapter, void *create, std::uint32_t kmt_adapter) override;
    d3d::UmdAdapter *tear_down9(void *device) override;

    /* What the next build12, caps and fill_table12 answer. */
    void answer(d3d::Hresult build, d3d::Hresult caps, d3d::Hresult fill);

    std::uint32_t    builds9() const;
    void            *device9() const;
    std::uint32_t    builds12() const;
    std::uint32_t    builds11() const;
    std::uint32_t    kmt_adapter() const;
    void            *built_block() const;
    d3d::RuntimeKind caps_runtime() const;
    std::uint32_t    caps_type() const;
    std::uint32_t    table_type() const;
    std::size_t      table_bytes() const;
    std::uint32_t    tear_downs() const;
    void            *torn_block() const;
    d3d::RuntimeKind torn_runtime() const;

private:
    d3d::Hresult     build_answer_;
    d3d::Hresult     caps_answer_;
    d3d::Hresult     fill_answer_;
    std::uint32_t    builds9_;
    d3d::UmdAdapter *built9_on_;
    std::uint32_t    builds12_;
    std::uint32_t    builds11_;
    std::uint32_t    kmt_adapter_;
    void            *built_block_;
    d3d::RuntimeKind caps_runtime_;
    std::uint32_t    caps_type_;
    std::uint32_t    table_type_;
    std::size_t      table_bytes_;
    std::uint32_t    tear_downs_;
    void            *torn_block_;
    d3d::RuntimeKind torn_runtime_;
};

}  // namespace av

#endif  // AV_RECORDING_HOST_DRIVER_HPP
