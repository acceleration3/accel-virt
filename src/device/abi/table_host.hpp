/* The host QEMU provides: every Host call forwarded to the glue's operations
 * table, with the threading contract checked against QEMU's big lock. */
#ifndef AV_TABLE_HOST_HPP
#define AV_TABLE_HOST_HPP

#include "device/abi/rvk_device.h"
#include "device/host/host.hpp"

namespace av {

class TableHost final : public Host {
public:
    explicit TableHost(const RvkHostOps &ops);

    void     log(const char *message) override;
    void     require_main_loop(const char *function) override;
    void     require_worker(const char *function) override;
    Status   start_thread(HostRoutine entry, void *context, void *&thread) override;
    void     join_thread(void *thread) override;
    Status   create_event(void *&event) override;
    void     destroy_event(void *event) override;
    void     signal_event(void *event) override;
    void     wait_event(void *event) override;
    Status   create_mutex(void *&mutex) override;
    void     destroy_mutex(void *mutex) override;
    void     lock_mutex(void *mutex) override;
    void     unlock_mutex(void *mutex) override;
    void     run_on_main_loop(HostRoutine routine, void *context) override;
    uint32_t gpu_count() override;
    void     gpu_uuid(uint32_t index, uint8_t *uuid) override;

private:
    void violated(const char *rule, const char *function);

    const RvkHostOps &ops_;
};

}  // namespace av

#endif  // AV_TABLE_HOST_HPP
