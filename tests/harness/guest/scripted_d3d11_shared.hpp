/* A Direct3D 11 the shared-clear scenario's tests drive: every call counted
 * and refusable, each target holding the colour it was last cleared to, and
 * the two ways a host fails it - a GPU that loses the device clearing a
 * target whose rows are no multiple of a pitch it can draw at, as NVIDIA's
 * did, and rows read back from where the writer did not put them. */
#ifndef AV_SCRIPTED_D3D11_SHARED_HPP
#define AV_SCRIPTED_D3D11_SHARED_HPP

#include "guest/probe/d3d11_shared_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedD3d11Shared final : public D3d11SharedEntry {
public:
    enum class Call : unsigned { create_device, create_target, clear, read_texel, removed_reason, release, count };
    static constexpr std::uint32_t most_targets = 4u;
    static constexpr std::int32_t  device_removed = static_cast<std::int32_t>(0x887A0005u);
    static constexpr std::int32_t  device_hung = static_cast<std::int32_t>(0x887A0006u);

    ScriptedD3d11Shared();
    ScriptedD3d11Shared(const ScriptedD3d11Shared &) = delete;
    ScriptedD3d11Shared &operator=(const ScriptedD3d11Shared &) = delete;

    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    /* A clear of a target whose BGRA rows are not a multiple of `bytes`
     * loses the device: every read after it is refused, and the device says
     * it hung. */
    void          lose_device_below(std::uint32_t bytes);
    /* A read of a target's last texel finds the bytes past its tight rows. */
    void          misplace_rows();
    std::uint32_t calls(Call call) const;
    bool          live() const;
    std::uint32_t width(std::uint32_t target) const;
    std::uint32_t height(std::uint32_t target) const;
    /* The texel a read last asked for. */
    std::uint32_t last_x() const;
    std::uint32_t last_y() const;

    std::int32_t create_device() override;
    std::int32_t create_target(std::uint32_t target, std::uint32_t width, std::uint32_t height) override;
    void         clear(std::uint32_t target, const std::uint8_t *colour) override;
    std::int32_t read_texel(std::uint32_t target, std::uint32_t x, std::uint32_t y, std::uint8_t *texel) override;
    std::int32_t removed_reason() override;
    void         release() override;

private:
    struct Target {
        std::uint32_t width;
        std::uint32_t height;
        /* Red first, as the clear was given it. */
        std::uint8_t  colour[4];
    };

    std::int32_t refusal(Call call);

    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t  fail_with_[static_cast<unsigned>(Call::count)];
    Target        targets_[most_targets];
    std::uint32_t lose_below_;
    std::uint32_t last_x_;
    std::uint32_t last_y_;
    bool          misplace_;
    bool          lost_;
    bool          live_;
};

}  // namespace av

#endif  // AV_SCRIPTED_D3D11_SHARED_HPP
