/* What every API's "the GPU reads and writes the application's own pages"
 * step proves, judged the same way for each (docs/host-memory.md,
 * "Application memory"): a pattern the application wrote through its own
 * pointer, copied by the GPU into another part of the same memory, and read
 * back through the application's pointer again. The entry of each API does
 * the API's part - D3D12's existing heap, OpenCL's CL_MEM_USE_HOST_PTR,
 * Vulkan's host pointer import - and hands what it read back here. */
#ifndef AV_APP_POINTER_CHECK_HPP
#define AV_APP_POINTER_CHECK_HPP

#include <stdint.h>

namespace av {

class ScenarioSteps;
struct AppPointerCheckProbe;

class AppPointerCheck {
public:
    /* One D3D12 placement unit: the source half and the destination half of
     * the application's memory are each this long. */
    static constexpr uint32_t bytes = 64u * 1024u;

    AppPointerCheck();
    AppPointerCheck(const AppPointerCheck &) = delete;
    AppPointerCheck &operator=(const AppPointerCheck &) = delete;

    /* What the application writes into the source half. */
    const uint8_t *pattern() const;
    /* Where the entry puts the destination half as the application's pointer
     * read it after the GPU's copy. */
    uint8_t       *copied();
    /* Three steps: `made`, the entry's own result; `aliased`, whether the
     * API's own mapping of the memory is the application's pointer (1) or
     * not; `copy`, how many bytes of the destination half are the pattern.
     * Nothing is judged past a refusal. */
    void           judge(ScenarioSteps &steps, const char *const names[3], int32_t result, uint32_t aliased);
    uint32_t       matching() const;

    bool invariants_hold() const;

private:
    uint8_t pattern_[bytes];
    uint8_t copied_[bytes];

    friend struct AppPointerCheckProbe;
};

}  // namespace av

#endif  // AV_APP_POINTER_CHECK_HPP
