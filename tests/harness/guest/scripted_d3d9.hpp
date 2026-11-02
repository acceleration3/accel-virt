#ifndef AV_SCRIPTED_D3D9_HPP
#define AV_SCRIPTED_D3D9_HPP

#include "guest/probe/d3d9_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedD3d9 final : public D3d9Entry {
public:
    static constexpr std::uint32_t vendor = 0x10DEu;
    static constexpr std::uint32_t device = 0x2204u;

    enum class Call : unsigned { create_device, adapter_ids, clear, read_back, present, release, count };

    ScriptedD3d9();
    ScriptedD3d9(const ScriptedD3d9 &) = delete;
    ScriptedD3d9 &operator=(const ScriptedD3d9 &) = delete;

    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    void          miscolour(unsigned channel);
    std::uint32_t calls(Call call) const;
    bool          live() const;
    void         *window() const;
    std::uint32_t width() const;

    std::int32_t create_device(void *window, std::uint32_t width, std::uint32_t height) override;
    std::int32_t adapter_ids(std::uint32_t &vendor_id, std::uint32_t &device_id) override;
    std::int32_t clear(const std::uint8_t *colour) override;
    std::int32_t read_back(std::uint8_t *pixels, std::uint32_t bytes) override;
    std::int32_t present() override;
    void         release() override;

private:
    std::int32_t refusal(Call call);

    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t  fail_with_[static_cast<unsigned>(Call::count)];
    std::uint8_t  cleared_[4];
    void         *window_;
    std::uint32_t width_;
    std::uint32_t height_;
    bool          live_;
    bool          cleared_once_;
    unsigned      miscoloured_;
};

}

#endif
