/* A library that loads, exports the symbol, and answers no table.
 *
 * The failure this exists to catch is not a crash: it is a device that comes
 * up as a plain virtio-gpu with no capset, where the guest finds no rvk
 * protocol and nothing anywhere says why. Realize must fail instead. */
#include "rvk_executor.h"

extern "C" __attribute__((visibility("default"))) const RvkExecutorOps *rvk_executor_ops(void)
{
    return nullptr;
}
