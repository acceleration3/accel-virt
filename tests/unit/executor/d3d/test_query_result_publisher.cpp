/* Query results written where the guest reads them. Every end is counted as
 * the guest counts them, whether or not it will be published; a query ended
 * again supersedes its last end; a pending query holds one reference, given
 * back when it is written, forgotten or dropped. Publishing itself is judged
 * on the host GPU, through DXVK, at the end of this file. */
#include "executor/d3d/query_result_publisher.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "harness/framework/test_registry.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/wait.h>
#include <unistd.h>

using av::QueryResultPublisher;

namespace {

class CountedQuery final : public ID3D11Query {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE   Release() override { return --references; }
    void STDMETHODCALLTYPE    GetDevice(ID3D11Device **device) override { *device = nullptr; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE    GetDataSize() override { return 8u; }
    void STDMETHODCALLTYPE    GetDesc(D3D11_QUERY_DESC *desc) override { *desc = D3D11_QUERY_DESC{}; }

    ULONG references = 1u;
};

alignas(8) std::uint8_t results[4u * QueryResultPublisher::slot_bytes];

}  // namespace

TEST(query_result_publisher_ended, holds_a_reference_to_each_pending_query_and_asks_for_a_flush)
{ QueryResultPublisher publisher;
  publisher.bind(results, 4u);
  CountedQuery query;
  CHECK(!publisher.needs_flush());
  CHECK(!publisher.pending());
  publisher.ended(1u, &query);
  CHECK_EQ(query.references, 2u);
  CHECK(publisher.pending());
  CHECK(publisher.needs_flush());
  publisher.flushed();
  CHECK(!publisher.needs_flush());
  CHECK(publisher.pending());   /* flushed is not published */
  publisher.release_all();
  CHECK_EQ(query.references, 1u);
  CHECK(!publisher.pending()); }

/* D3D11 reads only a query's latest end: a second end replaces the first and
 * gives its reference back. */
TEST(query_result_publisher_ended, a_query_ended_again_supersedes_its_last_end)
{ QueryResultPublisher publisher;
  publisher.bind(results, 4u);
  CountedQuery first, second;
  publisher.ended(1u, &first);
  publisher.ended(1u, &second);
  CHECK_EQ(first.references, 1u);
  CHECK_EQ(second.references, 2u);
  publisher.forget(1u);
  CHECK_EQ(second.references, 1u);
  CHECK(!publisher.pending());
  publisher.forget(1u);   /* nothing left of it: nothing */
  CHECK_EQ(second.references, 1u); }

/* Unbound, or with no query behind the end, nothing is held and nothing asks
 * for a flush; the end is still counted. */
TEST(query_result_publisher_ended, an_unbound_publisher_or_a_missing_query_holds_nothing)
{ QueryResultPublisher publisher;
  CountedQuery query;
  publisher.ended(1u, &query);
  CHECK_EQ(query.references, 1u);
  CHECK(!publisher.needs_flush());
  publisher.bind(results, 0u);   /* no slots is no binding */
  publisher.ended(1u, &query);
  CHECK_EQ(query.references, 1u);
  publisher.bind(results, 4u);
  publisher.ended(2u, nullptr);
  CHECK(!publisher.needs_flush());
  CHECK(!publisher.pending()); }

TEST(query_result_publisher_bind, a_new_binding_or_an_unbind_drops_every_pending_query)
{ QueryResultPublisher publisher;
  publisher.bind(results, 4u);
  CountedQuery one, two;
  publisher.ended(1u, &one);
  publisher.ended(2u, &two);
  publisher.bind(results, 2u);
  CHECK_EQ(one.references, 1u);
  CHECK_EQ(two.references, 1u);
  CHECK(!publisher.pending());
  publisher.ended(1u, &one);
  publisher.unbind();
  CHECK_EQ(one.references, 1u);
  publisher.ended(1u, &one);
  CHECK_EQ(one.references, 1u); }

TEST(query_result_publisher_ended, a_full_list_leaves_the_next_query_unpublished_and_unreferenced)
{ QueryResultPublisher publisher;
  publisher.bind(results, 4u);
  static CountedQuery queries[QueryResultPublisher::pending_capacity + 1u];
  for (std::uint32_t id = 0u; id <= QueryResultPublisher::pending_capacity; id++) {
      queries[id].references = 1u;
      publisher.ended(id + 1u, &queries[id]);
  }
  CHECK_EQ(queries[0].references, 2u);
  CHECK_EQ(queries[QueryResultPublisher::pending_capacity].references, 1u);
  publisher.release_all();
  for (const CountedQuery &query : queries) CHECK_EQ(query.references, 1u); }

/* ---- on the host GPU ------------------------------------------------------ */

namespace {

/* The first Vulkan physical device's UUID, which is how DXVK's adapter is found. */
bool first_gpu(std::uint8_t *uuid)
{
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = (1u << 22u) | (1u << 12u);   /* 1.1 */
    VkInstanceCreateInfo made = {};
    made.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    made.pApplicationInfo = &application;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&made, nullptr, &instance) != VK_SUCCESS) return false;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    std::uint32_t    count = 1u;
    const bool       found = vkEnumeratePhysicalDevices(instance, &count, &physical) >= 0 && count != 0u;
    if (found) {
        VkPhysicalDeviceIDProperties ids = {};
        ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 properties = {};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &ids;
        vkGetPhysicalDeviceProperties2(physical, &properties);
        std::memcpy(uuid, ids.deviceUUID, VK_UUID_SIZE);
    }
    vkDestroyInstance(instance, nullptr);
    return found;
}

/* What the child saw: whether it ran at all, and what the slot held. */
struct Published {
    std::uint32_t ran;
    std::uint32_t pending;
    std::uint64_t end;
    std::uint32_t size;
    std::uint32_t first_byte;
};

/* DXVK's own threads allocate from modules it unloads before exit, which
 * LeakSanitizer reports as an unknown module no suppression can name - so the
 * GPU's part runs in a child that leaves with _exit, and hands back what it
 * saw. */
Published publish_an_event_query()
{
    Published seen = {};
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::D3dRuntime runtime;
    std::uint8_t   uuid[VK_UUID_SIZE] = {};
    if (!runtime.open() || !first_gpu(uuid)) return seen;
    IDXGIAdapter1 *const adapter = runtime.adapter_for(uuid);
    if (adapter == nullptr) return seen;
    ID3D11Device        *device = nullptr;
    ID3D11DeviceContext *immediate = nullptr;
    std::uint32_t        level = 0u;
    const bool           made = runtime.create_device11(adapter, device, immediate, level) == S_OK;
    adapter->Release();
    if (!made) return seen;
    ID3D11DeviceContext4 *context = nullptr;
    D3D11_QUERY_DESC      desc = {};
    desc.Query = D3D11_QUERY_EVENT;
    ID3D11Query *query = nullptr;
    if (FAILED(immediate->QueryInterface(__uuidof(ID3D11DeviceContext4), reinterpret_cast<void **>(&context))) ||
        FAILED(device->CreateQuery(&desc, &query)))
        return seen;
    std::memset(results, 0, sizeof results);
    QueryResultPublisher publisher;
    publisher.bind(results, 4u);
    publisher.ended(9u, nullptr);   /* an end the guest counted that never ran */
    context->End(query);
    publisher.ended(6u, query);
    context->Flush();
    publisher.flushed();
    bool pending = true;
    for (std::uint32_t tries = 0u; tries < 5000u && pending; tries++) {
        pending = publisher.publish(context);
        const timespec pause = {0, 1000000};
        if (pending) nanosleep(&pause, nullptr);
    }
    const std::uint8_t *const slot = results + (6u % 4u) * QueryResultPublisher::slot_bytes;
    seen.ran = 1u;
    seen.pending = pending ? 1u : 0u;
    std::memcpy(&seen.end, slot, sizeof seen.end);
    std::memcpy(&seen.size, slot + 8u, sizeof seen.size);
    seen.first_byte = slot[QueryResultPublisher::data_at];
    return seen;
}

}  // namespace

/* An event query ended through DXVK on the host GPU, published into its slot
 * once the GPU has it: its data (TRUE), its size and, last, the count of the
 * end - the second the guest sent, though the first never ran. */
TEST(query_result_publisher_publish, writes_a_finished_query_s_result_and_its_end_count_into_its_slot)
{ int channel[2] = {-1, -1};
  CHECK_EQ(pipe(channel), 0);
  const pid_t child = fork();
  CHECK(child >= 0);
  if (child == 0) {
      const Published seen = publish_an_event_query();
      static_cast<void>(write(channel[1], &seen, sizeof seen));
      _exit(0);
  }
  close(channel[1]);
  Published seen = {};
  const ssize_t got = read(channel[0], &seen, sizeof seen);
  close(channel[0]);
  int status = 0;
  CHECK_EQ(waitpid(child, &status, 0), child);
  CHECK_EQ(got, static_cast<ssize_t>(sizeof seen));
  if (seen.ran == 0u) return;   /* no DXVK on a GPU here: nothing to judge */
  CHECK_EQ(seen.pending, 0u);
  CHECK(seen.end == 2u);
  CHECK_EQ(seen.size, 4u);   /* a BOOL */
  CHECK_EQ(seen.first_byte, 1u); }
