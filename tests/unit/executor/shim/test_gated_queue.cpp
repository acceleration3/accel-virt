#include "executor/shim/borrowed_queue.hpp"
#include "executor/shim/context_gate.hpp"
#include "executor/shim/gated_queue.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <thread>

namespace {

class FakeGate final : public av::ContextGate {
public:
    void enter() override
    {
        entered++;
        held = true;
    }
    void leave() override
    {
        left++;
        held = false;
    }
    bool inside() const override { return held; }

    std::uint32_t entered = 0u;
    std::uint32_t left = 0u;
    bool          held = false;
};

class FakeQueue final : public av::BorrowedQueue {
public:
    explicit FakeQueue(const FakeGate &gate) : gate_(gate) {}

    VkQueue  queue() const override { return reinterpret_cast<VkQueue>(std::uintptr_t{0x10}); }
    uint32_t family() const override { return 3u; }
    void     flush() override
    {
        flushes++;
        gated_at_flush = gate_.inside();
    }
    void lock() override { locks++; }
    void unlock() override { unlocks++; }

    std::uint32_t flushes = 0u;
    std::uint32_t locks = 0u;
    std::uint32_t unlocks = 0u;
    bool          gated_at_flush = false;

private:
    const FakeGate &gate_;
};

}

TEST(gated_queue, a_flush_from_a_thread_outside_the_context_gate_runs_inside_it)
{ FakeGate  gate;
  FakeQueue inner(gate);
  av::GatedQueue queue(inner, gate);
  queue.flush();
  CHECK_EQ(inner.flushes, 1u);
  CHECK(inner.gated_at_flush);
  CHECK_EQ(gate.entered, 1u);
  CHECK_EQ(gate.left, 1u);
  CHECK(!gate.held); }

TEST(gated_queue, a_flush_from_inside_the_context_gate_does_not_enter_it_again)
{ FakeGate  gate;
  FakeQueue inner(gate);
  av::GatedQueue queue(inner, gate);
  gate.enter();
  queue.flush();
  CHECK_EQ(inner.flushes, 1u);
  CHECK(inner.gated_at_flush);
  CHECK_EQ(gate.entered, 1u);
  CHECK_EQ(gate.left, 0u);
  CHECK(gate.held);
  gate.leave(); }

TEST(gated_queue, the_queue_lock_and_identity_pass_through_without_the_context_gate)
{ FakeGate  gate;
  FakeQueue inner(gate);
  av::GatedQueue queue(inner, gate);
  queue.lock();
  queue.unlock();
  CHECK_EQ(queue.family(), 3u);
  CHECK(queue.queue() == inner.queue());
  CHECK_EQ(inner.locks, 1u);
  CHECK_EQ(inner.unlocks, 1u);
  CHECK_EQ(gate.entered, 0u); }
