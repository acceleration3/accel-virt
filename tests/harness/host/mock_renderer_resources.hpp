/* What the renderer process gives its service, answered in the test's own
 * process: a guest region "maps" to the pointer the QEMU side's MockHost
 * registered it with, found through any duplicate of its descriptor, so the
 * renderer and the device see the same bytes; the executor is a table the
 * test built. Every descriptor the service is handed is counted until it is
 * closed, and every mapping until it is unmapped; one the service got
 * elsewhere - its library's dma-buf - is closed uncounted, but must be open. */
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

    /* `count` descriptors arrived with a datagram and are the service's now. */
    void          adopt(const int *descriptors, std::uint32_t count);
    /* The next load_executor answers not_found, as a path that does not open. */
    void          refuse_executor();
    std::uint32_t live_descriptors() const;
    std::uint32_t live_mappings() const;
    /* Fence lines holding a guest notifier. */
    std::uint32_t interrupt_lines() const;
    std::uint32_t executor_loads() const;
    /* The path the last load named, empty before the first. */
    const char   *executor_path() const;

private:
    MockHost             &qemu_;
    const AvExecutorOps &executor_;
    int                   held_[max_held];   /* -1 for a free slot */
    std::uint32_t         mappings_;
    int                   lines_[max_lines];
    bool                  refuse_executor_;
    std::uint32_t         loads_;
    char                  path_[max_path];
};

}  // namespace av

#endif  // AV_MOCK_RENDERER_RESOURCES_HPP
