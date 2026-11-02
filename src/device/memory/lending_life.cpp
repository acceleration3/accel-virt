#include "device/memory/lending_life.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

LendingLife::LendingLife() : refs_(1u), given_back_(0u)
{
    AV_SCOPE("LendingLife::LendingLife");
}

void LendingLife::give_back()
{
    AV_SCOPE("LendingLife::give_back");
    __atomic_store_n(&given_back_, 1u, __ATOMIC_SEQ_CST);
}

void LendingLife::hold()
{
    AV_SCOPE("LendingLife::hold");
    refs_++;
}

uint32_t LendingLife::let_go()
{
    AV_SCOPE("LendingLife::let_go");
    return --refs_;
}

bool LendingLife::given_back() const
{
    AV_SCOPE("LendingLife::given_back");
    return __atomic_load_n(&given_back_, __ATOMIC_SEQ_CST) != 0u;
}

}  // namespace av
