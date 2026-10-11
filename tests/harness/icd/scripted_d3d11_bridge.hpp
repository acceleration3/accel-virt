/* The tests' D3d11Bridge: one Direct3D 11 device and the few resources a test
 * names, each with what it says of itself and the bytes of the subresource
 * the test copies through it. A read hands a staging copy of those bytes to
 * the rows, as a map of a staging resource would; a write hands zeroed
 * staging and copies what the rows wrote back into them. It counts each
 * object's references and every read and write, and can be told to refuse
 * the next one, as a device that will not make a staging resource does. */
#ifndef AV_SCRIPTED_D3D11_BRIDGE_HPP
#define AV_SCRIPTED_D3D11_BRIDGE_HPP

#include "icd/opencl/d3d11_bridge.hpp"
#include "icd/opencl/d3d11_resource.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedD3d11Bridge final : public clicd::D3d11Bridge {
public:
    static constexpr std::uint32_t max_resources = 8u;
    static constexpr std::size_t   staging_bytes = 4096u;

    ScriptedD3d11Bridge();

    /* `device` is an ID3D11Device from now on. */
    void name_device(void *device);
    /* `resource` is a resource of `device` described as `described`; `bytes`
     * is its subresource, laid out a row every `row_pitch` bytes and a slice
     * every `slice_pitch`, `size` of them in all. */
    void name_resource(void *resource, void *device, const clicd::D3d11Resource &described, std::uint8_t *bytes,
                       std::size_t size, std::size_t row_pitch, std::size_t slice_pitch);
    /* The next read or write answers CL_OUT_OF_RESOURCES and maps nothing. */
    void refuse_next();

    bool   device(void *object) override;
    bool   describe(void *object, void *device, clicd::D3d11Resource &described) override;
    void   hold(void *object) override;
    void   let_go(void *object) override;
    cl_int read(void *device, void *resource, std::uint32_t subresource, clicd::D3d11Rows &rows) override;
    cl_int write(void *device, void *resource, std::uint32_t subresource, clicd::D3d11Rows &rows) override;

    /* References held to `object` and not let go of. */
    std::int32_t  references(const void *object) const;
    std::uint32_t reads() const;
    std::uint32_t writes() const;
    /* The subresource the last read or write named. */
    std::uint32_t subresource() const;

private:
    struct Resource {
        void                *object;
        void                *device;
        clicd::D3d11Resource described;
        std::uint8_t        *bytes;
        std::size_t          size;
        std::size_t          row_pitch;
        std::size_t          slice_pitch;
    };

    Resource    *named(const void *object);
    /* Whether the next read or write is refused, which it then is not again. */
    bool         refused();

    void         *device_;
    Resource      resources_[max_resources];
    std::uint32_t count_;
    const void   *held_[max_resources + 1u];
    std::int32_t  references_[max_resources + 1u];
    std::uint8_t  staging_[staging_bytes];
    std::uint32_t reads_;
    std::uint32_t writes_;
    std::uint32_t subresource_;
    bool          refuse_;
};

}  // namespace av

#endif  // AV_SCRIPTED_D3D11_BRIDGE_HPP
