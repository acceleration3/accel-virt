#ifndef AV_SCRIPTED_OVERLAY_HPP
#define AV_SCRIPTED_OVERLAY_HPP

#include "guest/probe/overlay_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedOverlay final : public OverlayEntry {
public:
    enum class Call : unsigned { create_device, supports_overlays, overlay_support, create_swap_chain, present,
                                 composition, release, count };

    ScriptedOverlay();
    ScriptedOverlay(const ScriptedOverlay &) = delete;
    ScriptedOverlay &operator=(const ScriptedOverlay &) = delete;

    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    void          answer(bool overlays, std::uint32_t support, std::uint32_t mode);
    std::uint32_t calls(Call call) const;
    bool          live() const;
    std::int32_t  x() const;
    std::uint32_t width() const;

    std::int32_t create_device() override;
    std::int32_t supports_overlays(bool &supported) override;
    std::int32_t overlay_support(std::uint32_t format, std::uint32_t &flags) override;
    std::uint32_t asked_format(std::uint32_t nth) const;
    std::int32_t create_swap_chain(std::int32_t x, std::int32_t y, std::uint32_t width, std::uint32_t height) override;
    std::int32_t present(const std::uint8_t *colour) override;
    std::int32_t composition(std::uint32_t &mode) override;
    void         release() override;

private:
    std::int32_t refusal(Call call);

    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t  fail_with_[static_cast<unsigned>(Call::count)];
    bool          overlays_;
    std::uint32_t support_;
    std::uint32_t mode_;
    bool          live_;
    std::int32_t  x_;
    std::uint32_t width_;
    std::uint32_t formats_[4] = {};
};

}

#endif
