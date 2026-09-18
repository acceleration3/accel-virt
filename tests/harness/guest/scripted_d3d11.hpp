/* A Direct3D 11 the probe's tests drive (feature 047, US1): every call
 * recorded, every call refusable, and a frame that comes back the colour it
 * was cleared to unless the test says otherwise. Nothing of D3D11 is here but
 * its shape - what is being tested is the probe's sequence and what it judges.
 */
#ifndef AV_SCRIPTED_D3D11_HPP
#define AV_SCRIPTED_D3D11_HPP

#include "guest/suite/d3d11_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedD3d11 final : public D3d11Entry {
public:
    /* The feature level a D3D11 device made on this driver reports: 11_0, as
     * DXVK answers on a Vulkan 1.3 device. */
    static constexpr std::uint32_t feature_level_11_0 = 0xB000u;
    static constexpr std::uint64_t adapter = 0x1234ABCDu;

    enum class Call : unsigned { create_device, adapter_luid, create_render_target, clear, read_back, present,
                                 release, count };

    ScriptedD3d11();
    ScriptedD3d11(const ScriptedD3d11 &) = delete;
    ScriptedD3d11 &operator=(const ScriptedD3d11 &) = delete;

    /* `call` answers `result` from its `nth` use on. */
    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    /* The frame comes back one pixel short of the colour. */
    /* The frame comes back with one pixel's `channel` off by more than a
     * driver's rounding: which channel matters, because each is compared on
     * its own. */
    void          miscolour(unsigned channel);
    std::uint32_t calls(Call call) const;
    /* Whether a device is live: what says the probe released what it made. */
    bool          live() const;

    std::int32_t create_device(void *window, std::uint32_t width, std::uint32_t height,
                               std::uint32_t &feature_level) override;
    std::int32_t adapter_luid(std::uint64_t &luid) override;
    std::int32_t create_render_target() override;
    void         clear(const std::uint8_t *colour) override;
    std::int32_t read_back(std::uint8_t *pixels, std::uint32_t bytes) override;
    std::int32_t present() override;
    void         release() override;

    /* The window the device was made on, and its frame's size. */
    void        *window() const;
    std::uint32_t width() const;

private:
    /* The result `call` is to answer now, 0 for none. */
    std::int32_t refusal(Call call);

    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t  fail_with_[static_cast<unsigned>(Call::count)];
    std::uint8_t  cleared_[4];
    void         *window_;
    std::uint32_t width_;
    std::uint32_t height_;
    bool          live_;
    bool          target_;
    unsigned      miscoloured_;
};

}  // namespace av

#endif  // AV_SCRIPTED_D3D11_HPP
