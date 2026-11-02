#ifndef AV_SCRIPTED_D3D11_DEFERRED_HPP
#define AV_SCRIPTED_D3D11_DEFERRED_HPP

#include "guest/probe/d3d11_deferred_entry.hpp"

#include <cstdint>

namespace av {

class ScriptedD3d11Deferred final : public D3d11DeferredEntry {
public:
    enum class Call : unsigned {
        create_device,
        create_buffers,
        create_contexts,
        write_constants,
        discard_geometry,
        append_geometry,
        finish,
        execute,
        present,
        read_geometry,
        release,
        count
    };
    static constexpr std::uint32_t most_contexts = 4u;
    static constexpr std::uint32_t most_constants = 8u;
    static constexpr std::uint32_t head_bytes = 256u;

    ScriptedD3d11Deferred();
    ScriptedD3d11Deferred(const ScriptedD3d11Deferred &) = delete;
    ScriptedD3d11Deferred &operator=(const ScriptedD3d11Deferred &) = delete;

    void          fail(Call call, std::uint32_t nth, std::int32_t result);
    void          commit_at_finish(std::uint32_t nth, std::int64_t bytes);
    void          lose_appends();
    void          lag_executes();
    std::uint32_t calls(Call call) const;
    std::uint32_t finishes_on(std::uint32_t context) const;
    bool          live() const;
    void         *window() const;
    std::uint32_t constant_size(std::uint32_t buffer) const;
    std::uint32_t constant_count() const;
    std::uint32_t geometry_bytes() const;
    std::uint32_t readback_bytes() const;
    std::uint32_t contexts() const;

    std::int32_t  create_device(void *window, std::uint32_t width, std::uint32_t height) override;
    std::int32_t  create_buffers(const std::uint32_t *sizes, std::uint32_t count, std::uint32_t geometry_bytes,
                                 std::uint32_t readback_bytes) override;
    std::int32_t  create_contexts(std::uint32_t count) override;
    std::int32_t  write_constants(std::uint32_t context, std::uint32_t buffer, std::uint8_t value) override;
    std::int32_t  discard_geometry(std::uint32_t context, std::uint32_t bytes, std::uint8_t value) override;
    std::int32_t  append_geometry(std::uint32_t context, std::uint32_t offset, std::uint32_t bytes,
                                  std::uint8_t value) override;
    std::int32_t  finish(std::uint32_t context) override;
    std::int32_t  execute(std::uint32_t context) override;
    std::int32_t  present() override;
    std::int32_t  read_geometry(std::uint8_t *bytes, std::uint32_t count) override;
    std::uint64_t committed_bytes() override;
    void          release() override;

private:
    struct Writes {
        std::uint8_t bytes[head_bytes];
        bool         written[head_bytes];
    };

    std::int32_t refusal(Call call);
    void         record(std::uint32_t context, std::uint32_t offset, std::uint32_t bytes, std::uint8_t value);
    void         apply(const Writes &list);

    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    std::uint32_t fail_at_[static_cast<unsigned>(Call::count)];
    std::int32_t  fail_with_[static_cast<unsigned>(Call::count)];
    Writes        recording_[most_contexts];
    Writes        lists_[most_contexts];
    Writes        previous_;
    std::uint8_t  geometry_[head_bytes];
    std::uint32_t finishes_on_[most_contexts];
    std::uint32_t constant_sizes_[most_constants];
    std::uint32_t constant_count_;
    std::uint32_t geometry_bytes_;
    std::uint32_t readback_bytes_;
    std::uint32_t contexts_;
    std::uint32_t commit_finish_;
    std::int64_t  commit_bytes_;
    std::uint64_t committed_;
    void         *window_;
    bool          lose_appends_;
    bool          lag_;
    bool          live_;
};

}

#endif
