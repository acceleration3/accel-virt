#include "executor/vram_gate.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/host_apis.hpp"
#include "executor/vram_ledger.hpp"

#include <string.h>

namespace av {

namespace {

constexpr uint32_t uuid_bytes = 16u;

void *gate_open(void *self, const uint8_t *gpu_uuid)
{
    AV_SCOPE("gate_open");
    return static_cast<VramGate *>(self)->open(gpu_uuid);
}

void gate_close(void *self, void *account)
{
    AV_SCOPE("gate_close");
    static_cast<VramGate *>(self)->close(static_cast<VramAccount *>(account));
}

int gate_take(void *account, uint64_t bytes)
{
    AV_SCOPE("gate_take");
    return account == nullptr || static_cast<VramAccount *>(account)->take(bytes) ? 1 : 0;
}

void gate_give(void *account, uint64_t bytes)
{
    AV_SCOPE("gate_give");
    if (account != nullptr) static_cast<VramAccount *>(account)->give(bytes);
}

uint64_t gate_free_bytes(void *account)
{
    AV_SCOPE("gate_free_bytes");
    return account != nullptr ? static_cast<VramAccount *>(account)->free_bytes() : UINT64_MAX;
}

uint64_t gate_cap(void *account)
{
    AV_SCOPE("gate_cap");
    return account != nullptr ? static_cast<VramAccount *>(account)->cap() : UINT64_MAX;
}

}

VramGate::VramGate(HostApis &apis, BlockSource &blocks) : apis_(apis), abi_{}, lock_(), accounts_(blocks), leaked_(0u)
{
    AV_SCOPE("VramGate::VramGate");
    abi_.size = sizeof(AvVramGate);
    abi_.self = this;
    abi_.open = gate_open;
    abi_.take = gate_take;
    abi_.give = gate_give;
    abi_.free_bytes = gate_free_bytes;
    abi_.cap = gate_cap;
    abi_.close = gate_close;
}

const AvVramGate &VramGate::abi() const
{
    AV_SCOPE("VramGate::abi");
    return abi_;
}

VramAccount *VramGate::open(const uint8_t *gpu_uuid)
{
    AV_SCOPE("VramGate::open");
    if (gpu_uuid == nullptr) return nullptr;
    VramLedger *ledger = nullptr;
    for (uint32_t index = 0u; index < apis_.gpu_count() && ledger == nullptr; index++) {
        uint8_t uuid[uuid_bytes] = {};
        apis_.gpu_uuid(index, uuid);
        if (memcmp(uuid, gpu_uuid, uuid_bytes) == 0) ledger = apis_.vram_ledger(index);
    }
    if (ledger == nullptr) return nullptr;
    const ThreadLockHold held(lock_);
    VramAccount *account = nullptr;
    return accounts_.create(account, *ledger) == Status::ok ? account : nullptr;
}

void VramGate::close(VramAccount *account)
{
    AV_SCOPE("VramGate::close");
    if (account == nullptr) return;
    const ThreadLockHold held(lock_);
    if (accounts_.find(account) == nullptr) return;
    leaked_ += account->close();
    static_cast<void>(accounts_.destroy(account));
}

uint32_t VramGate::live() const
{
    AV_SCOPE("VramGate::live");
    const ThreadLockHold held(lock_);
    return accounts_.live();
}

uint64_t VramGate::leaked() const
{
    AV_SCOPE("VramGate::leaked");
    const ThreadLockHold held(lock_);
    return leaked_;
}

bool VramGate::invariants_hold() const
{
    AV_SCOPE("VramGate::invariants_hold");
    const ThreadLockHold held(lock_);
    return accounts_.invariants_hold();
}

}
