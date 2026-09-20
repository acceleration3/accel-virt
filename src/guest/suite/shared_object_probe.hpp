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

class ClEntry;
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
    /* More than any OpenCL device's extension string, which is one line. */
    static constexpr uint32_t cl_extension_bytes = 4096u;

    /* `cl` is the OpenCL beside this process, or null. With one, the probe
     * runs the other half of the interop matrix: OpenCL opens the handle
     * Vulkan exported and reads what Vulkan wrote into it, which is the only
     * place in the guest where two APIs meet over one allocation (feature 053,
     * T019). Without one the Vulkan steps run alone and say so. */
    SharedObjectProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan, ClEntry *cl);
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
    /* The same create at a spread of private-data lengths, to find where the
     * kernel stops taking it. Each answers one of three things and they are
     * now distinguishable: 0 taken, 0xC000000D from dxgkrnl before the driver
     * was called at all, or 0xC00000BB from the driver saying it has no shape
     * for that length. Measured rather than judged, because most of these
     * lengths are ones no caller uses and a sweep that failed the run for them
     * would fail every run for ever (feature 053). */
    void carrier_size_steps(uint32_t device);
    /* The buffer the fill is a transfer onto, and the memory types it can be
     * bound to: made before the allocation because those types are what the
     * allocation must come from. */
    bool target_step(ProbeDevice &device);
    /* Memory declared exportable, which is what makes the miniport mint. */
    bool exportable_step(ProbeDevice &device, VkDeviceMemory &memory);
    /* Known bytes into the allocation, which is what the OpenCL read is
     * judged against. */
    void fill_step(ProbeDevice &device, VkDeviceMemory memory);
    bool handle_step(ProbeDevice &device, VkDeviceMemory memory, void *&handle);
    void released_step(ProbeDevice &device, VkDeviceMemory memory);
    /* The other half of the matrix, with the handle Vulkan just exported:
     * OpenCL opens it and reads back what Vulkan wrote. Every one of them is
     * skipped rather than failed on a process with no OpenCL, because this is
     * the Vulkan probe and the loader is not always beside it - and on a
     * driver that does not report `cl_khr_external_memory_win32`, because
     * there is then nothing an application would be allowed to ask for. */
    void opencl_steps(void *handle);

    Kmt          &kmt_;
    VulkanEntry  &vulkan_;
    ClEntry      *cl_;
    /* The buffer bound over the shared memory, and the memory types it takes. */
    VkBuffer      target_;
    uint32_t      target_types_;
    bool          filled_;
    ScenarioSteps steps_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_PROBE_HPP
