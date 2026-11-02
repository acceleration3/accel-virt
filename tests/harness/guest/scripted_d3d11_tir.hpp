#ifndef AV_SCRIPTED_D3D11_TIR_HPP
#define AV_SCRIPTED_D3D11_TIR_HPP

#include "guest/probe/d3d11_tir_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedD3d11Tir final : public D3d11TirEntry {
public:
    enum class Call : unsigned { create_device, create_pipeline, draw, read_back, removed_reason, release, count };
    static constexpr std::uint32_t most_extent = 64u;
    static constexpr std::int32_t  device_hung = static_cast<std::int32_t>(0x887A0006u);

    ScriptedD3d11Tir();
    ScriptedD3d11Tir(const ScriptedD3d11Tir &) = delete;
    ScriptedD3d11Tir &operator=(const ScriptedD3d11Tir &) = delete;

    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    void          rasterize_at_one_sample();
    void          lose();
    void          set_layered(bool layered);
    std::uint32_t calls(Call call) const;
    bool          live() const;
    std::uint32_t extent() const;
    std::uint32_t samples() const;
    std::uint32_t vertices() const;

    std::int32_t create_device() override;
    std::int32_t create_pipeline(std::uint32_t extent, std::uint32_t samples) override;
    std::int32_t draw(const float *positions, std::uint32_t count) override;
    std::int32_t read_back(std::uint32_t *texels, std::uint32_t count) override;
    std::int32_t removed_reason() override;
    bool         layered() override;
    void         release() override;

private:
    std::int32_t refusal(Call call);

    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t  fail_with_[static_cast<unsigned>(Call::count)];
    std::uint32_t texels_[most_extent * most_extent];
    std::uint32_t extent_;
    std::uint32_t samples_;
    std::uint32_t vertices_;
    bool          one_sample_;
    bool          layered_;
    bool          lost_;
    bool          live_;
};

}

#endif
