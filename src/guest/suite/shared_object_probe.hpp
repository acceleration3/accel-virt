/* rvkvkprobe's proof that a shared object's identity is minted in the kernel
 * (feature 053, US1), over whatever Vulkan it is handed.
 *
 * Four calls in `GdiKmt` exist only for this and nothing else in the guest
 * reaches them: `create_shared_allocation`, which asks the miniport to mint an
 * identity; `share_object`, which turns it into an NT handle;
 * `open_shared_object`, which another process opens that handle with; and
 * `close_shared_object`. Three of the four run in *one* process - allocating
 * memory the application declared exportable, and asking for its handle - so
 * that is what this probe does.
 *
 * The fourth needs a second process, and is deliberately not faked here: a
 * handle this process minted resolves in its own table and never reaches
 * `open_shared_object`, so a one-process probe that claimed to exercise it
 * would be proving nothing. When the second process exists it is a sibling
 * method - `open_steps(handle)` on this class, with the handle arriving from
 * outside - rather than a change to what is below.
 *
 * Steps, as tools/guest/run_guest_suite.py reads them: `reported`, whether the
 * driver offers VK_KHR_external_memory_win32 at all - a host with no fd
 * sharing offers it to nobody, and the rest then do not run; `exportable`, the
 * allocation that asks for an identity; `handle`, the NT handle minted over
 * it; `released`, everything given back. Every failure carries the NTSTATUS
 * of the last kernel call, which is what says *which* of the four refused. */
#ifndef AV_SHARED_OBJECT_PROBE_HPP
#define AV_SHARED_OBJECT_PROBE_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class GuestReport;
class Kmt;
class ProbeDevice;
class VulkanEntry;

class SharedObjectProbe {
public:
    /* Whole pages, which is what a shared object is measured in. */
    static constexpr uint64_t bytes = 4u * 4096u;
    /* More device extensions than any host lists; a longer list is read this
     * far, since all this asks is whether one name is among them. */
    static constexpr uint32_t max_extensions = 512u;

    SharedObjectProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan);
    SharedObjectProbe(const SharedObjectProbe &) = delete;
    SharedObjectProbe &operator=(const SharedObjectProbe &) = delete;

    /* The export half: the three calls one process reaches. */
    void run();

private:
    /* Whether the driver reports VK_KHR_external_memory_win32 on `device`'s
     * physical device. */
    bool reported_step(ProbeDevice &device);
    /* The kernel call the whole feature rests on, asked directly rather than
     * through the ICD: an ordinary 48-byte create on this probe's own device,
     * then the 88-byte two-way one the ICD needs. Two steps because the answer
     * that matters is which of them the kernel takes - a guest that refuses
     * only the second says the shape is wrong, and one that refuses both says
     * something else is (feature 053). */
    void carrier_steps();
    /* Memory declared exportable, which is what makes the miniport mint. */
    bool exportable_step(ProbeDevice &device, VkDeviceMemory &memory);
    bool handle_step(ProbeDevice &device, VkDeviceMemory memory, void *&handle);
    void released_step(ProbeDevice &device, VkDeviceMemory memory);

    Kmt          &kmt_;
    VulkanEntry  &vulkan_;
    ScenarioSteps steps_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_PROBE_HPP
