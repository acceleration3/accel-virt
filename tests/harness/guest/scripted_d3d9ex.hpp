#ifndef AV_SCRIPTED_D3D9EX_HPP
#define AV_SCRIPTED_D3D9EX_HPP

#include "guest/probe/d3d9ex_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowFrame;

class ScriptedD3d9Ex final : public D3d9ExEntry {
public:
    enum class Call : unsigned { create_device, create_swap_chain, draw, present, wait_for_blank, release, count };

    explicit ScriptedD3d9Ex(ScriptedWindowFrame &window);
    ScriptedD3d9Ex(const ScriptedD3d9Ex &) = delete;
    ScriptedD3d9Ex &operator=(const ScriptedD3d9Ex &) = delete;

    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    void          arrive_after(std::uint32_t count);
    void          lose_the_frame();
    std::uint32_t calls(Call call) const;
    bool          live() const;
    void         *window() const;
    std::uint32_t width() const;
    std::uint32_t height() const;

    std::int32_t create_device(void *window) override;
    std::int32_t create_swap_chain(void *window, std::uint32_t width, std::uint32_t height) override;
    std::int32_t draw(const std::uint8_t *colour) override;
    std::int32_t present(std::uint32_t width, std::uint32_t height) override;
    std::int32_t wait_for_blank() override;
    void         release() override;

private:
    std::int32_t refusal(Call call);
    void         arrive();

    ScriptedWindowFrame &window_frame_;
    std::uint32_t        calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t        fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t         fail_with_[static_cast<unsigned>(Call::count)];
    std::uint8_t         drawn_[4];
    void                *window_;
    std::uint32_t        width_;
    std::uint32_t        height_;
    std::uint32_t        blanks_to_arrival_;
    bool                 lost_;
    bool                 pending_;
    bool                 live_;
};

}

#endif
