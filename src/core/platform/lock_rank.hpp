/* The lock order, declared once. A thread may only acquire a lock whose rank is
 * strictly greater than every lock it already holds, so no two locks can ever
 * be taken in opposite orders. Each named rank is added here by the feature
 * that introduces its lock, in acquisition order. */
#ifndef AV_LOCK_RANK_HPP
#define AV_LOCK_RANK_HPP

#include <stdint.h>

namespace av {

enum class LockRank : uint16_t {
    doorbell_table = 70u,     /* DoorbellTable, taken before the queues it names (feature 049) */
    native_fence_table = 75u,  /* NativeFenceTable, a leaf above the queues it is asked beside (feature 049) */
    hw_queue_table = 80u,     /* HwQueueTable, which looks contexts up in ContextTable */
    context_table = 90u,      /* ContextTable, which looks devices up in DeviceTable */
    device_table = 100u,       /* DeviceTable, which calls into AllocationTable */
    rvk_control_requests = 105u, /* wait mutex: an rvk context's setup, teardown or chunk request sequence,
                                    which resolves its allocations in AllocationTable */
    host_window = 107u,        /* HostWindow's page map: a leaf, taken inside a window chunk's request sequence */
    allocation_table = 110u,
    shared_objects = 115u,     /* SharedObjectTable: minted inside the allocation table's create,
                                  and opened beside it from the device table (feature 053) */
    present_payload = 117u,    /* wait mutex: PresentCopier's payload, held while the request carrying
                                  it waits for the device's answer */
    control_exchanges = 120u,  /* wait mutex: one control request and its answer at a time, whoever
                                  asks - a present's copy beside an rvk context's request sequence */
    dpc = 150u,                /* spin lock: the adapter's DPC, which KeInsertQueueDpc may run on two
                                  processors at once; completions must reach dxgkrnl one at a time, in order */
    rvk_context_slots = 190u,  /* spin lock: which rvk contexts are live, around a submission and the DPC's serve */
    execution_node = 200u,     /* spin lock: SubmitCommand, PreemptCommand and the DPC */
    hw_queue_contexts = 205u,  /* spin lock: what each hardware queue runs on, read at DISPATCH (feature 049) */
    hw_queue_submissions = 210u, /* spin lock: SubmitCommandToHwQueue, DestroyHwQueue, ResetHwEngine and the DPC */
    scanout = 215u,            /* spin lock: DisplayScanout, between a VidPn commit's configure and the DPC's
                                  refresh, which sends under it */
    control_queue = 220u,      /* spin lock: virtio-gpu control requests and the DPC's harvest */
    cursor_queue = 230u,       /* spin lock: virtio-gpu cursor requests and the DPC's harvest */
    rvk_context = 240u,        /* spin lock: an rvk context's submission, answers and the DPC's serve */
    log_budget = 250u,         /* spin lock: a leaf, taken while the DPC already holds the locks above */
};

}  // namespace av

#endif  // AV_LOCK_RANK_HPP
