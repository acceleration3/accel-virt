#ifndef AV_VRAM_GATE_HPP
#define AV_VRAM_GATE_HPP

#include "core/support/object_pool.hpp"
#include "device/shim/posix_lock.hpp"
#include "executor/av_vram_gate.h"
#include "executor/vram_account.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class ByteLedger;
class HostApis;

class GateAccount {
public:
    GateAccount(ByteLedger &vram_ledger, ByteLedger *host_ledger);
    GateAccount(const GateAccount &) = delete;
    GateAccount &operator=(const GateAccount &) = delete;

    VramAccount &vram();
    VramAccount &host();

private:
    VramAccount vram_;
    VramAccount host_;
};

class VramGate {
public:
    VramGate(HostApis &apis, BlockSource &blocks);
    VramGate(const VramGate &) = delete;
    VramGate &operator=(const VramGate &) = delete;

    const AvVramGate &abi() const;
    GateAccount      *open(const uint8_t *gpu_uuid);
    void              close(GateAccount *account);
    uint32_t          live() const;
    uint64_t          leaked() const;
    uint64_t          leaked_host() const;

    bool invariants_hold() const;

private:
    HostApis                &apis_;
    AvVramGate               abi_;
    mutable PosixLock        lock_;
    ObjectPool<GateAccount>  accounts_;
    uint64_t                 leaked_;
    uint64_t                 leaked_host_;
};

}

#endif
