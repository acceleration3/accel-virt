/* Every host handle this executor made, and nothing else.
 *
 * A handle crosses the wire as a number, and on the host that number is an
 * attacker's: the generated decoder used to hand it to the driver as a pointer,
 * so a guest naming any address had it dereferenced. Found by
 * tests/fuzz/fuzz_executor_vulkan.cpp on its first run, in vkDestroyInstance.
 *
 * So the host answers a handle only if it made it. Everything a guest can name
 * is in here, put there when the command that created it succeeded and taken
 * out when the command that destroyed it ran. A number that is not is not a
 * handle: it is a frame this host refuses. */
#ifndef AV_HANDLE_REGISTRY_HPP
#define AV_HANDLE_REGISTRY_HPP

#include <stdint.h>

namespace av {

class HandleRegistry {
public:
    /* A Vulkan context of any size lives well inside this: it is objects the
     * guest created, not commands it sent, and a guest that wants more is
     * refused rather than served. */
    static constexpr uint32_t max_handles = 4096u;

    HandleRegistry();
    HandleRegistry(const HandleRegistry &) = delete;
    HandleRegistry &operator=(const HandleRegistry &) = delete;

    /* Remembers `handle`, which this executor just made. False when full,
     * which the caller turns into a refusal rather than a silent forget. */
    [[nodiscard]] bool remember(uint64_t handle);
    /* Whether this executor made `handle`. The null handle is always known:
     * the API defines it and a guest may legitimately send it. */
    bool               knows(uint64_t handle) const;
    /* Forgets one it made. Nothing for one it did not. */
    void               forget(uint64_t handle);
    void               forget_all();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    uint64_t handles_[max_handles];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_HANDLE_REGISTRY_HPP
