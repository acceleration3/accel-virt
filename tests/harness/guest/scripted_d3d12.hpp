/* A Direct3D 12 the probe's tests drive (feature 047, US2): every call
 * recorded, every call refusable, and a frame that comes back the colour the
 * list cleared it to unless the test says otherwise. */
#ifndef AV_SCRIPTED_D3D12_HPP
#define AV_SCRIPTED_D3D12_HPP

#include "guest/probe/d3d12_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedD3d12 final : public D3d12Entry {
public:
    /* D3D_FEATURE_LEVEL_12_0, as vkd3d-proton answers on a Vulkan 1.3
     * device. */
    static constexpr std::uint32_t feature_level_12_0 = 0xC000u;
    static constexpr std::uint64_t adapter = 0x1234ABCDu;

    enum class Call : unsigned { create_device, adapter_luid, create_queue, create_render_target, clear_and_copy,
                                 wait, read_back, release, count };

    ScriptedD3d12();
    ScriptedD3d12(const ScriptedD3d12 &) = delete;
    ScriptedD3d12 &operator=(const ScriptedD3d12 &) = delete;

    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    /* The frame comes back one pixel short of the colour. */
    /* The frame comes back with one pixel's `channel` off by more than a
     * driver's rounding: which channel matters, because each is compared on
     * its own. */
    void          miscolour(unsigned channel);
    std::uint32_t calls(Call call) const;
    bool          live() const;
    std::uint32_t width() const;

    std::int32_t create_device(std::uint32_t &feature_level) override;
    std::int32_t adapter_luid(std::uint64_t &luid) override;
    std::int32_t create_queue() override;
    std::int32_t create_render_target(std::uint32_t width, std::uint32_t height) override;
    std::int32_t clear_and_copy(const std::uint8_t *colour) override;
    std::int32_t wait() override;
    std::int32_t read_back(std::uint8_t *pixels, std::uint32_t bytes) override;
    void         release() override;

private:
    std::int32_t refusal(Call call);

    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t  fail_with_[static_cast<unsigned>(Call::count)];
    std::uint8_t  cleared_[4];
    std::uint32_t width_;
    std::uint32_t height_;
    bool          live_;
    bool          queue_;
    bool          target_;
    bool          copied_;
    bool          waited_;
    unsigned      miscoloured_;
};

}  // namespace av

#endif  // AV_SCRIPTED_D3D12_HPP
