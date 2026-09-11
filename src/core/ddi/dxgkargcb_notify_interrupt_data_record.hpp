/* DXGKARGCB_NOTIFY_INTERRUPT_DATA, the argument of DxgkCbNotifyInterrupt. A
 * DDI record: the union of per-type structures is laid out as the four UINTs
 * the DMA interrupts use - DmaCompleted is SubmissionFenceId, NodeOrdinal,
 * EngineOrdinal; DmaPreempted is PreemptionFenceId, LastCompletedFenceId,
 * NodeOrdinal, EngineOrdinal - and the rest of the union; Flags is the
 * DXGKCB_NOTIFY_INTERRUPT_DATA_FLAGS Value. src/km/ddi_record_layout.cpp
 * proves each offset. */
#ifndef AV_DXGKARGCB_NOTIFY_INTERRUPT_DATA_RECORD_HPP
#define AV_DXGKARGCB_NOTIFY_INTERRUPT_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargcbNotifyInterruptDataRecord {
    int32_t  InterruptType;
    uint32_t InterruptTypePadding;
    uint32_t FenceId;
    uint32_t Argument1;
    uint32_t Argument2;
    uint32_t Argument3;
    uint32_t UnionRemainder[12];
    uint32_t Flags;
    uint32_t Padding;
};

}  // namespace av

#endif  // AV_DXGKARGCB_NOTIFY_INTERRUPT_DATA_RECORD_HPP
