#include "executor/d3d/d3d12_pipeline_compiles.hpp"
#include "executor/d3d/d3d12_pipeline_job.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/teardown_budget.hpp"
#include "harness/executor/fake_device.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/framework/test_registry.hpp"

#include <atomic>
#include <cstdint>

using av::D3d12PipelineCompiles;
using av::D3d12PipelineJob;

namespace {

class Root final : public ID3D12RootSignature {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE   Release() override { return --references; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void **) override { return E_NOTIMPL; }

    ULONG references = 1u;
};

av::HostHeap blocks;

D3D12_COMPUTE_PIPELINE_STATE_DESC compute() { return D3D12_COMPUTE_PIPELINE_STATE_DESC{}; }

D3d12PipelineJob &started(D3d12PipelineCompiles &compiles, Root &root, std::uint32_t id)
{
    D3d12PipelineJob *const job = compiles.vacant();
    CHECK(job != nullptr);
    job->describe(id, &root, compute());
    compiles.start(*job);
    return *job;
}

}

TEST(d3d12_pipeline_compiles, a_new_set_has_every_job_vacant_and_none_pending)
{ D3d12PipelineCompiles compiles(blocks);
  CHECK(compiles.vacant() != nullptr);
  CHECK(compiles.pending(1u) == nullptr);
  CHECK(compiles.oldest() == nullptr);
  CHECK(compiles.invariants_hold()); }

TEST(d3d12_pipeline_compiles, a_vacant_job_taken_and_started_is_pending_by_its_id_and_vacant_again_once_collected)
{ D3d12PipelineCompiles compiles(blocks);
  Root                  root;
  compiles.bind(nullptr);
  D3d12PipelineJob *const first = compiles.vacant();
  D3d12PipelineJob &job = started(compiles, root, 5u);
  CHECK(&job == first);
  CHECK(compiles.pending(5u) == &job);
  CHECK(compiles.pending(6u) == nullptr);
  CHECK(compiles.oldest() == &job);
  CHECK(compiles.vacant() != &job);
  CHECK(compiles.invariants_hold());
  CHECK(compiles.collect(job) == nullptr);
  CHECK(compiles.pending(5u) == nullptr);
  CHECK(compiles.oldest() == nullptr);
  CHECK(compiles.vacant() == first);
  CHECK_EQ(root.references, 1u);
  CHECK(compiles.invariants_hold()); }

TEST(d3d12_pipeline_compiles, the_oldest_job_is_the_first_started_and_every_job_is_taken_when_none_is_vacant)
{ D3d12PipelineCompiles compiles(blocks);
  Root                  root;
  compiles.bind(nullptr);
  D3d12PipelineJob *jobs[D3d12PipelineCompiles::job_count] = {};
  for (std::uint32_t index = 0u; index < D3d12PipelineCompiles::job_count; index++)
      jobs[index] = &started(compiles, root, index + 1u);
  CHECK(compiles.vacant() == nullptr);
  CHECK(compiles.oldest() == jobs[0]);
  CHECK(compiles.invariants_hold());
  CHECK(compiles.collect(*jobs[0]) == nullptr);
  CHECK(compiles.oldest() == jobs[1]);
  CHECK(compiles.vacant() == jobs[0]);
  for (std::uint32_t index = 1u; index < D3d12PipelineCompiles::job_count; index++)
      CHECK(compiles.collect(*jobs[index]) == nullptr);
  CHECK(compiles.oldest() == nullptr);
  CHECK_EQ(root.references, 1u);
  CHECK(compiles.invariants_hold()); }

TEST(d3d12_pipeline_compiles, a_stopped_set_starts_its_threads_again_with_the_next_job)
{ D3d12PipelineCompiles compiles(blocks);
  Root                  root;
  compiles.bind(nullptr);
  D3d12PipelineJob &first = started(compiles, root, 1u);
  CHECK(compiles.collect(first) == nullptr);
  compiles.stop();
  D3d12PipelineJob &second = started(compiles, root, 2u);
  CHECK(compiles.pending(2u) == &second);
  CHECK(compiles.collect(second) == nullptr);
  CHECK(compiles.vacant() != nullptr);
  CHECK(compiles.invariants_hold()); }

namespace {

std::atomic<bool> compile_entered(false);
std::atomic<bool> compile_may_end(false);

void hold_compile(void *)
{
    compile_entered.store(true);
    while (!compile_may_end.load()) {
    }
}

}

TEST(d3d12_pipeline_compiles, a_compile_still_running_at_the_budget_is_reported_and_one_that_ends_lets_the_set_settle)
{ D3d12PipelineCompiles compiles(blocks);
  Root                  root;
  av::FakeDevice        device;
  device.compiling = hold_compile;
  compile_entered = false;
  compile_may_end = false;
  compiles.bind(&device);
  D3d12PipelineJob &job = started(compiles, root, 1u);
  while (!compile_entered.load()) {
  }
  av::MonotonicClock clock;
  av::TeardownBudget short_budget(clock, 20u * 1000u * 1000u);
  CHECK(!compiles.settle(short_budget));
  compile_may_end = true;
  av::TeardownBudget long_budget(clock, 2000u * 1000u * 1000u);
  CHECK(compiles.settle(long_budget));
  CHECK(compiles.collect(job) == nullptr);
  compiles.stop(); }
