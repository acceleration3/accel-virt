#ifndef AV_MOCK_RENDERER_RESOURCES_HPP
#define AV_MOCK_RENDERER_RESOURCES_HPP

#include "device/renderer/renderer_resources.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockHost;

class MockRendererResources final : public RendererResources {
public:
    static constexpr std::uint32_t max_lines = 64u;
    static constexpr std::size_t   max_path = 128u;
    static constexpr std::uint32_t max_held = 256u;

    MockRendererResources(MockHost &qemu, const AvExecutorOps &executor);
    ~MockRendererResources();
    MockRendererResources(const MockRendererResources &) = delete;
    MockRendererResources &operator=(const MockRendererResources &) = delete;

    [[nodiscard]] Status map_region(int descriptor, std::uint64_t size, void *&base) override;
    void                 unmap_region(void *base, std::uint64_t size) override;
    void                 close_descriptor(int descriptor) override;
    void                 take_interrupt_lines(const int *descriptors, std::uint32_t count) override;
    void                 drop_interrupt_lines() override;
    [[nodiscard]] Status load_executor(const char *path, std::uint32_t length, const AvExecutorOps *&ops) override;

    void          adopt(const int *descriptors, std::uint32_t count);
    void          refuse_executor();
    void          load_executor_without_table();
    std::uint32_t live_descriptors() const;
    std::uint32_t live_mappings() const;
    std::uint32_t interrupt_lines() const;
    std::uint32_t executor_loads() const;
    const char   *executor_path() const;

private:
    MockHost             &qemu_;
    const AvExecutorOps &executor_;
    int                   held_[max_held];
    std::uint32_t         mappings_;
    int                   lines_[max_lines];
    bool                  refuse_executor_;
    bool                  load_nothing_;
    std::uint32_t         loads_;
    char                  path_[max_path];
};

}

#endif
